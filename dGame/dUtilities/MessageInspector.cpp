#include "MessageInspector.h"
#include "MasterPackets.h"

#include <chrono>
#include <cstring>
#include <optional>
#include <vector>

#include "BitStream.h"
#include "BitStreamUtils.h"
#include "Entity.h"
#include "Game.h"
#include "GameMessageDecoder.h"
#include "GameMessages.h"
#include "Logger.h"
#include "master/MessageCapture.h"
#include "MessageType/Client.h"
#include "MessageType/Master.h"
#include "PlayerManager.h"
#include "ServiceType.h"
#include "dServer.h"
#include "dZoneManager.h"

namespace {
	using Clock = std::chrono::steady_clock;

	constexpr size_t MAX_CAPTURES = 8;
	// Per capture: at most this many messages a second, and this many waiting to be sent
	constexpr uint32_t MAX_PER_SECOND = 200;
	constexpr size_t MAX_PENDING = 1000;
	constexpr auto SEND_INTERVAL = std::chrono::milliseconds(250);
	// Sent even when nothing was captured, so the dashboard knows the capture is still running
	constexpr auto HEARTBEAT_INTERVAL = std::chrono::seconds(5);
	constexpr size_t BATCH_ENTRIES = 100;
	constexpr size_t BATCH_BYTES = 48 * 1024;
	constexpr size_t BATCHES_PER_SEND = 2;

	// Game messages to a client start with the packet header, the object ID and the message ID
	constexpr size_t GAME_MESSAGE_HEADER_BYTES = 8 + sizeof(LWOOBJID) + sizeof(MessageType::Game);

	struct Capture {
		MessageCaptureControl control;
		SystemAddress sysAddr;
		Clock::time_point until;
		MessageCaptureQueue queue{ MAX_PENDING, MAX_PER_SECOND };
		uint32_t nextSequence{ 1 };
		Clock::time_point lastSent{};
	};

	std::vector<Capture> g_Captures;

	int64_t NowMs() {
		return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	}

	void Send(const Capture& capture, eMessageCaptureStatus status, std::vector<MessageCaptureEntry> entries = {}, uint32_t dropped = 0, eMessageCaptureEnd reason = {}) {
		if (!Game::server) return;
		MessageCaptureData data;
		data.captureId = capture.control.captureId;
		data.status = status;
		data.characterId = capture.control.characterId;
		data.zoneId = Game::zoneManager ? Game::zoneManager->GetZoneID().GetMapID() : 0;
		data.instanceId = Game::server->GetInstanceID();
		data.cloneId = Game::zoneManager ? Game::zoneManager->GetZoneID().GetCloneID() : 0;
		data.reason = reason;
		data.dropped = dropped;
		data.entries = std::move(entries);

		MasterPackets::SendToMaster(data);
	}

	// Send what is waiting: a couple of batches per call (the rest goes next time), or everything when `all`
	void Flush(Capture& capture, Clock::time_point now, bool all) {
		if (!all && now - capture.lastSent < SEND_INTERVAL) return;
		bool sent = false;
		for (size_t i = 0; all || i < BATCHES_PER_SEND; i++) {
			auto batch = capture.queue.Take(BATCH_ENTRIES, BATCH_BYTES);
			const auto dropped = capture.queue.TakeDropped();
			if (batch.empty() && dropped == 0) break;
			Send(capture, eMessageCaptureStatus::ENTRIES, std::move(batch), dropped);
			sent = true;
		}
		if (!sent && !all && now - capture.lastSent >= HEARTBEAT_INTERVAL) {
			Send(capture, eMessageCaptureStatus::ENTRIES);
			sent = true;
		}
		if (sent) capture.lastSent = now;
	}

	void SetHook();

	void End(size_t index, eMessageCaptureEnd reason) {
		auto& capture = g_Captures[index];
		Flush(capture, Clock::now(), true);
		Send(capture, eMessageCaptureStatus::ENDED, {}, 0, reason);
		LOG("Message capture %u of character %llu ended (%i)", capture.control.captureId, capture.control.characterId, static_cast<int>(reason));
		g_Captures.erase(g_Captures.begin() + index);
		SetHook();
	}

	/**
	 * Add a message to every capture that wants it. Decoding happens once, and only for messages a capture keeps.
	 * `address` is who the message goes to (or, for a broadcast, who it skips).
	 */
	void Record(eMessageDirection direction, const SystemAddress& address, bool broadcast, LWOOBJID objectId, uint16_t messageId,
		const unsigned char* data, uint32_t bits) {
		const auto now = Clock::now();
		const auto timeMs = NowMs();
		std::optional<std::string> decoded;
		for (auto& capture : g_Captures) {
			// A broadcast reaches everyone except the address given (when one is given)
			const bool reaches = broadcast ? capture.sysAddr != address : capture.sysAddr == address;
			if (!reaches || !capture.control.Wants(direction, messageId)) continue;

			const auto bytes = (bits + 7) / 8;
			if (!decoded) {
				decoded.emplace();
				const bool toServer = direction == eMessageDirection::TO_SERVER;
				if (GameMessageDecoder::CanDecode(static_cast<MessageType::Game>(messageId), toServer)) {
					RakNet::BitStream payload(const_cast<unsigned char*>(data), bytes, true);
					const auto fields = GameMessageDecoder::Decode(static_cast<MessageType::Game>(messageId), toServer, payload);
					if (fields) {
						*decoded = fields->dump();
						if (decoded->size() > MessageCapture::MAX_DECODED) decoded->clear();
					}
				}
			}

			MessageCaptureEntry entry;
			entry.sequence = capture.nextSequence++;
			entry.timeMs = timeMs;
			entry.direction = direction;
			entry.messageId = messageId;
			entry.objectId = objectId;
			entry.bits = bits;
			entry.payload.assign(reinterpret_cast<const char*>(data), std::min<size_t>(bytes, MessageCapture::MAX_PAYLOAD));
			entry.decoded = *decoded;
			capture.queue.Push(std::move(entry), now);
		}
	}

	// Packets the server sends: only game messages are kept
	void OnSend(const RakNet::BitStream& bitStream, const SystemAddress& sysAddr, bool broadcast) {
		const auto totalBits = bitStream.GetNumberOfBitsUsed();
		if (totalBits < GAME_MESSAGE_HEADER_BYTES * 8) return;
		auto* data = bitStream.GetData();
		RakNet::BitStream stream(data, bitStream.GetNumberOfBytesUsed(), false);
		LWOOBJID objectId{};
		MessageType::Game messageId{};
		if (!GameMessages::NetGameMsg::ReadPacketHeader(stream, objectId, messageId)) return;

		Record(eMessageDirection::TO_CLIENT, sysAddr, broadcast, objectId, static_cast<uint16_t>(messageId), data + GAME_MESSAGE_HEADER_BYTES,
			static_cast<uint32_t>(totalBits - GAME_MESSAGE_HEADER_BYTES * 8));
	}

	// Watch sent packets only while something is captured
	void SetHook() {
		MessageInspector::g_Capturing = !g_Captures.empty();
		if (!Game::server) return;
		Game::server->SetSendObserver(MessageInspector::g_Capturing ? dServer::SendObserver(OnSend) : nullptr);
	}
}

namespace MessageInspector {
	bool g_Capturing = false;

	void Control(const MessageCaptureControl& control) {
		if (control.action != eMessageCaptureControl::START && control.action != eMessageCaptureControl::STOP) return;
		const auto existing = std::ranges::find_if(g_Captures, [&](const Capture& c) { return c.control.captureId == control.captureId; });
		if (control.action == eMessageCaptureControl::STOP) {
			if (existing != g_Captures.end()) End(existing - g_Captures.begin(), eMessageCaptureEnd::STOPPED);
			return;
		}

		// Only the world the character is in captures it
		auto* player = PlayerManager::GetPlayer(control.characterId);
		if (!player || control.seconds == 0) return;
		if (existing == g_Captures.end() && g_Captures.size() >= MAX_CAPTURES) {
			LOG("Not starting message capture %u: %zu captures are already running here", control.captureId, g_Captures.size());
			return;
		}

		Capture& capture = existing != g_Captures.end() ? *existing : g_Captures.emplace_back();
		capture.control = control;
		capture.sysAddr = player->GetSystemAddress();
		capture.until = Clock::now() + std::chrono::seconds(std::min(control.seconds, MessageCapture::MAX_SECONDS));
		capture.lastSent = Clock::now();
		LOG("Capturing game messages of character %llu for %u seconds (capture %u)", control.characterId, control.seconds, control.captureId);
		Send(capture, eMessageCaptureStatus::STARTED);
		SetHook();
	}

	void RecordReceived(const SystemAddress& sysAddr, LWOOBJID objectId, MessageType::Game messageId, const RakNet::BitStream& payload) {
		Record(eMessageDirection::TO_SERVER, sysAddr, false, objectId, static_cast<uint16_t>(messageId), payload.GetData(),
			static_cast<uint32_t>(payload.GetNumberOfBitsUsed()));
	}

	void Update() {
		if (!g_Capturing) return;
		const auto now = Clock::now();
		for (size_t i = g_Captures.size(); i-- > 0;) {
			auto& capture = g_Captures[i];
			const auto* player = PlayerManager::GetPlayer(capture.control.characterId);
			if (!player || player->GetSystemAddress() != capture.sysAddr) End(i, eMessageCaptureEnd::PLAYER_LEFT);
			else if (now >= capture.until) End(i, eMessageCaptureEnd::TIME_LIMIT);
			else Flush(capture, now, false);
		}
	}
}
