#ifndef __MESSAGECAPTURE__H__
#define __MESSAGECAPTURE__H__

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <deque>
#include <string>
#include <string_view>
#include <vector>

#include "BitStream.h"
#include "BitStreamUtils.h"
#include "MessageType/Master.h"
#include "dCommonVars.h"

/**
 * The dashboard's game message inspector: staff capture the game messages one online player sends and receives.
 *
 * MESSAGE_CAPTURE_CONTROL (dashboard -> master -> every world) starts or stops a capture; only the world holding the
 * character acts on it. MESSAGE_CAPTURE_DATA (world -> master -> dashboard) reports that the capture started, carries
 * batches of captured messages, and says when and why it ended there. Worlds stop a capture on their own at its time
 * limit, so one can never be left running.
 */
enum class eMessageDirection : uint8_t {
	TO_SERVER, // sent by the player's client
	TO_CLIENT, // sent to the player's client (including broadcasts they receive)
};

enum class eMessageCaptureControl : uint8_t {
	START,
	STOP,
	ARM,    // packet capture (PacketCapture.h): every server records the target's packets into `slot`
	DISARM, // packet capture: stop recording into `slot`
};

// What a packet capture (ARM) records
enum class eCaptureTarget : uint8_t {
	CHARACTER,  // one character, from the moment it is picked in a world
	ACCOUNT,    // everything of one account, from its login (every character)
	EVERYTHING, // all traffic on all servers
};

enum class eMessageCaptureStatus : uint8_t {
	STARTED,  // the world holding the character started capturing
	ENTRIES,  // captured messages (also sent empty every few seconds while capturing)
	ENDED,    // the world stopped capturing; `reason` says why
	PACKETS,  // a batch of recorded packets (PacketCapture.h) from any server: `packets`, `slots`, `source`
};

enum class eMessageCaptureEnd : uint8_t {
	STOPPED,     // staff stopped it
	TIME_LIMIT,  // its time ran out
	PLAYER_LEFT, // the player left this world (logged out or changed zones)
};

namespace MessageCapture {
	// A capture runs at most this long
	constexpr uint32_t MAX_SECONDS = 15 * 60;
	// Captured bytes kept per message; larger messages are cut (the entry keeps their full size)
	constexpr uint16_t MAX_PAYLOAD = 2048;
	// Decoded fields (JSON text) kept per message
	constexpr uint16_t MAX_DECODED = 8192;
	// Message IDs in a filter list
	constexpr uint16_t MAX_FILTER = 256;
	// Packet captures armed at once (one bit each in a packet record's mask)
	constexpr uint8_t MAX_SLOTS = 8;
	// Characters of an account a packet capture follows
	constexpr uint16_t MAX_CHARACTERS = 64;
	// One batch of packet records
	constexpr uint32_t MAX_PACKET_BATCH = 8 * 1024 * 1024;

	// Lowercase hex, two digits per byte
	inline std::string ToHex(std::string_view bytes) {
		static constexpr char DIGITS[] = "0123456789abcdef";
		std::string hex;
		hex.reserve(bytes.size() * 2);
		for (const unsigned char byte : bytes) {
			hex += DIGITS[byte >> 4];
			hex += DIGITS[byte & 0x0f];
		}
		return hex;
	}

	inline void WriteString(RakNet::BitStream& stream, const std::string& text, uint16_t max) {
		const auto length = static_cast<uint16_t>(std::min<size_t>(text.size(), max));
		stream.Write(length);
		if (length) stream.Write(text.data(), length);
	}

	inline bool ReadString(RakNet::BitStream& stream, std::string& text, uint16_t max) {
		uint16_t length{};
		if (!stream.Read(length) || length > max) return false;
		text.resize(length);
		return length == 0 || stream.Read(text.data(), length);
	}

	inline void WriteIds(RakNet::BitStream& stream, const std::vector<uint16_t>& ids) {
		const auto count = static_cast<uint16_t>(std::min<size_t>(ids.size(), MAX_FILTER));
		stream.Write(count);
		for (uint16_t i = 0; i < count; i++) stream.Write(ids[i]);
	}

	inline bool ReadIds(RakNet::BitStream& stream, std::vector<uint16_t>& ids) {
		uint16_t count{};
		if (!stream.Read(count) || count > MAX_FILTER) return false;
		ids.resize(count);
		for (auto& id : ids) if (!stream.Read(id)) return false;
		return true;
	}
}

// MESSAGE_CAPTURE_CONTROL payload
struct MessageCaptureControl : public LUBitStream {
	MessageCaptureControl() : LUBitStream(ServiceType::MASTER, MessageType::Master::MESSAGE_CAPTURE_CONTROL) {}

	uint32_t captureId{};
	eMessageCaptureControl action{};
	LWOOBJID characterId{};
	uint32_t seconds{};           // START: how long to capture (capped at MessageCapture::MAX_SECONDS)
	bool toServer{ true };        // START: capture what the client sends
	bool toClient{ true };        // START: capture what the client receives
	std::vector<uint16_t> only;   // START: capture only these message IDs (empty: all)
	std::vector<uint16_t> skip;   // START: never capture these message IDs
	// ARM / DISARM (packet capture); `seconds` is its time limit and captureId its id
	uint8_t slot{};                       // 0 to MAX_SLOTS - 1: the bit this capture has in packet records
	eCaptureTarget target{};
	uint32_t accountId{};                 // ACCOUNT, CHARACTER: the account
	std::string accountName;              // its name, for packets that name the account instead (logins, session keys)
	std::vector<LWOOBJID> characterIds;   // ACCOUNT: its characters; CHARACTER: the one

	void Serialize(RakNet::BitStream& stream) const override {
		stream.Write(captureId);
		stream.Write(action);
		stream.Write(characterId);
		stream.Write(std::min(seconds, MessageCapture::MAX_SECONDS));
		stream.Write<uint8_t>(toServer);
		stream.Write<uint8_t>(toClient);
		MessageCapture::WriteIds(stream, only);
		MessageCapture::WriteIds(stream, skip);
		stream.Write(slot);
		stream.Write(target);
		stream.Write(accountId);
		MessageCapture::WriteString(stream, accountName, 64);
		const auto count = static_cast<uint16_t>(std::min<size_t>(characterIds.size(), MessageCapture::MAX_CHARACTERS));
		stream.Write(count);
		for (uint16_t i = 0; i < count; i++) stream.Write(characterIds[i]);
	}

	bool Deserialize(RakNet::BitStream& stream) override {
		uint8_t server{}, client{};
		if (!stream.Read(captureId) || !stream.Read(action) || !stream.Read(characterId) || !stream.Read(seconds) ||
			!stream.Read(server) || !stream.Read(client)) return false;
		seconds = std::min(seconds, MessageCapture::MAX_SECONDS);
		toServer = server != 0;
		toClient = client != 0;
		if (!MessageCapture::ReadIds(stream, only) || !MessageCapture::ReadIds(stream, skip)) return false;
		uint16_t count{};
		if (!stream.Read(slot) || slot >= MessageCapture::MAX_SLOTS || !stream.Read(target) || target > eCaptureTarget::EVERYTHING ||
			!stream.Read(accountId) || !MessageCapture::ReadString(stream, accountName, 64) || !stream.Read(count) || count > MessageCapture::MAX_CHARACTERS) return false;
		characterIds.resize(count);
		for (auto& id : characterIds) if (!stream.Read(id)) return false;
		return true;
	}

	// Whether a message passes this capture's filters
	bool Wants(eMessageDirection direction, uint16_t messageId) const {
		if (!(direction == eMessageDirection::TO_SERVER ? toServer : toClient)) return false;
		if (std::ranges::find(skip, messageId) != skip.end()) return false;
		return only.empty() || std::ranges::find(only, messageId) != only.end();
	}
};

// One captured game message
struct MessageCaptureEntry {
	uint32_t sequence{};       // per capture, from 1; gaps mean messages were dropped
	int64_t timeMs{};          // Unix time in milliseconds
	eMessageDirection direction{};
	uint16_t messageId{};      // MessageType::Game
	LWOOBJID objectId{};       // the object the message is addressed to
	uint32_t bits{};           // full size of the message's fields, in bits
	std::string payload;       // the fields' bytes (at most MAX_PAYLOAD)
	std::string decoded;       // the fields as JSON when the server has a typed struct for the message, else empty

	// Roughly how many bytes this takes in a packet, for batching
	size_t WireSize() const { return 32 + payload.size() + decoded.size(); }

	void Serialize(RakNet::BitStream& stream) const {
		stream.Write(sequence);
		stream.Write(timeMs);
		stream.Write(direction);
		stream.Write(messageId);
		stream.Write(objectId);
		stream.Write(bits);
		MessageCapture::WriteString(stream, payload, MessageCapture::MAX_PAYLOAD);
		MessageCapture::WriteString(stream, decoded, MessageCapture::MAX_DECODED);
	}

	bool Deserialize(RakNet::BitStream& stream) {
		return stream.Read(sequence) && stream.Read(timeMs) && stream.Read(direction) && stream.Read(messageId) &&
			stream.Read(objectId) && stream.Read(bits) &&
			MessageCapture::ReadString(stream, payload, MessageCapture::MAX_PAYLOAD) &&
			MessageCapture::ReadString(stream, decoded, MessageCapture::MAX_DECODED);
	}
};

// MESSAGE_CAPTURE_DATA payload
struct MessageCaptureData : public LUBitStream {
	MessageCaptureData() : LUBitStream(ServiceType::MASTER, MessageType::Master::MESSAGE_CAPTURE_DATA) {}

	static constexpr uint16_t MAX_ENTRIES = 500;

	uint32_t captureId{};
	eMessageCaptureStatus status{};
	LWOOBJID characterId{};
	uint32_t zoneId{};
	uint32_t instanceId{};
	eMessageCaptureEnd reason{}; // ENDED
	uint32_t dropped{};          // messages left out since the last batch (over the rate or buffer limit)
	std::vector<MessageCaptureEntry> entries;
	uint32_t cloneId{};          // the world's clone (a property's owner), 0 elsewhere
	// PACKETS: records (PacketRecord.h) packed one after another, the capture id of each mask bit when they were
	// recorded, which server recorded them, how many there are and how many that server left out since its last batch
	uint8_t source{};            // eCaptureSource
	std::array<uint32_t, MessageCapture::MAX_SLOTS> slots{};
	uint32_t packetCount{};
	uint32_t packetsDropped{};
	std::string packets;

	void Serialize(RakNet::BitStream& stream) const override {
		stream.Write(captureId);
		stream.Write(status);
		stream.Write(characterId);
		stream.Write(zoneId);
		stream.Write(instanceId);
		stream.Write(reason);
		stream.Write(dropped);
		const auto count = static_cast<uint16_t>(std::min<size_t>(entries.size(), MAX_ENTRIES));
		stream.Write(count);
		for (uint16_t i = 0; i < count; i++) entries[i].Serialize(stream);
		stream.Write(cloneId);
		stream.Write(source);
		for (const auto id : slots) stream.Write(id);
		stream.Write(packetCount);
		stream.Write(packetsDropped);
		const auto length = static_cast<uint32_t>(std::min<size_t>(packets.size(), MessageCapture::MAX_PACKET_BATCH));
		stream.Write(length);
		if (length) stream.Write(packets.data(), length);
	}

	bool Deserialize(RakNet::BitStream& stream) override {
		uint16_t count{};
		if (!stream.Read(captureId) || !stream.Read(status) || !stream.Read(characterId) || !stream.Read(zoneId) ||
			!stream.Read(instanceId) || !stream.Read(reason) || !stream.Read(dropped) || !stream.Read(count) || count > MAX_ENTRIES) return false;
		entries.resize(count);
		for (auto& entry : entries) if (!entry.Deserialize(stream)) return false;
		uint32_t length{};
		if (!stream.Read(cloneId) || !stream.Read(source)) return false;
		for (auto& id : slots) if (!stream.Read(id)) return false;
		if (!stream.Read(packetCount) || !stream.Read(packetsDropped) || !stream.Read(length) || length > MessageCapture::MAX_PACKET_BATCH) return false;
		packets.resize(length);
		return length == 0 || stream.Read(packets.data(), length);
	}
};

/**
 * Captured messages waiting to be sent, bounded by count and by rate so a busy player can't flood master and the
 * dashboard. What goes over either limit is counted as dropped instead.
 */
class MessageCaptureQueue {
public:
	using Clock = std::chrono::steady_clock;

	MessageCaptureQueue(size_t maxPending, uint32_t maxPerSecond) : m_MaxPending(maxPending), m_MaxPerSecond(maxPerSecond) {}

	// False if the entry was dropped
	bool Push(MessageCaptureEntry entry, Clock::time_point now) {
		if (now - m_WindowStart >= std::chrono::seconds(1)) {
			m_WindowStart = now;
			m_InWindow = 0;
		}
		if (m_InWindow >= m_MaxPerSecond || m_Pending.size() >= m_MaxPending) {
			m_Dropped++;
			return false;
		}
		m_InWindow++;
		m_Pending.push_back(std::move(entry));
		return true;
	}

	// The oldest entries, up to maxEntries and about maxBytes (always at least one when any are waiting)
	std::vector<MessageCaptureEntry> Take(size_t maxEntries, size_t maxBytes) {
		std::vector<MessageCaptureEntry> batch;
		size_t bytes = 0;
		while (!m_Pending.empty() && batch.size() < maxEntries) {
			const auto size = m_Pending.front().WireSize();
			if (!batch.empty() && bytes + size > maxBytes) break;
			bytes += size;
			batch.push_back(std::move(m_Pending.front()));
			m_Pending.pop_front();
		}
		return batch;
	}

	// Entries dropped since the last call
	uint32_t TakeDropped() {
		const auto dropped = m_Dropped;
		m_Dropped = 0;
		return dropped;
	}

	size_t Pending() const { return m_Pending.size(); }

private:
	size_t m_MaxPending;
	uint32_t m_MaxPerSecond;
	std::deque<MessageCaptureEntry> m_Pending;
	Clock::time_point m_WindowStart{};
	uint32_t m_InWindow{};
	uint32_t m_Dropped{};
};

#endif  //!__MESSAGECAPTURE__H__
