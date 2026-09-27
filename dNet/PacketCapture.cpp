#include "PacketCapture.h"

#include <algorithm>
#include <chrono>
#include <deque>
#include <map>
#include <unordered_map>
#include <vector>

#include "BitStream.h"
#include "BitStreamUtils.h"
#include "dConfig.h"
#include "Game.h"
#include "GeneralUtils.h"
#include "Logger.h"
#include "MasterPackets.h"
#include "PacketDecoder.h"
#include "master/InstanceMigration.h"
#include "master/MessageCapture.h"
#include "MessageIdentifiers.h"
#include "MessageType/Master.h"
#include "MessageType/Auth.h"
#include "MessageType/Client.h"
#include "RakPeer.h"
#include "RakPeerInterface.h"
#include "ServiceType.h"
#include "dServer.h"

// Servers always have a logger; tests may not
#define CAPTURE_LOG(...) do { if (Game::logger) LOG(__VA_ARGS__); } while (0)

namespace {
	using Clock = std::chrono::steady_clock;
	using Master = MessageType::Master;

	// Packets kept per connection before it is known whose it is (a login, a world's session check), for account
	// captures only: when the account turns out to be captured they are added in front
	constexpr size_t PENDING_RECORDS = 32;
	constexpr size_t PENDING_BYTES = 64 * 1024;
	constexpr auto PENDING_FOR = std::chrono::seconds(60);
	// Zone transfer requests remembered to tie their answer to the player
	constexpr size_t MAX_REQUESTS = 256;

	struct Slot {
		bool armed{};
		uint32_t captureId{};
		eCaptureTarget target{};
		uint32_t accountId{};
		std::string accountName;
		std::vector<LWOOBJID> characterIds;
		Clock::time_point until{};
	};

	struct Binding {
		uint32_t accountId{};
		std::string accountName;
		LWOOBJID characterId{};
		uint8_t mask{}; // account and character slots this connection belongs to
	};

	struct Pending {
		std::string records;
		size_t count{};
		Clock::time_point since{};
	};

	struct Chunk {
		std::string records;
		std::array<uint32_t, MessageCapture::MAX_SLOTS> slots{};
		uint32_t count{};
	};

	ServiceType g_ServerType{};
	eCaptureSource g_Source{};
	RakPeerInterface* g_Peer{};
	RakPeerInterface* g_MasterLink{};
	uint16_t g_Zone{}, g_Instance{};
	uint32_t g_Clone{};
	std::vector<uint64_t> g_Ignored;

	std::array<Slot, MessageCapture::MAX_SLOTS> g_Slots;
	uint8_t g_EverythingMask{}; // slots capturing everything
	uint8_t g_SubjectMask{};    // slots capturing an account or a character
	uint8_t g_AccountMask{};    // slots capturing an account

	std::unordered_map<uint64_t, Binding> g_Bindings;
	std::vector<uint64_t> g_Unbind;       // connections that closed; forgotten at the next receive
	std::unordered_map<uint64_t, Pending> g_Pending;
	std::map<uint64_t, uint8_t> g_Requests; // zone transfer request -> mask
	uint64_t g_Scope{};                     // connection whose packet is being handled

	PacketCapture::Settings g_Settings;
	Chunk g_Chunk;
	std::deque<Chunk> g_Sealed;
	uint64_t g_SealedBytes{};
	uint32_t g_Dropped{};                   // since the last batch
	uint32_t g_Seq{};
	Clock::time_point g_ChunkStarted{};
	PacketCapture::Sink g_Sink;
	PacketCapture::Stats g_Stats;

	uint64_t Key(const SystemAddress& address) {
		return (static_cast<uint64_t>(address.binaryAddress) << 16) | address.port;
	}

	int64_t NowUs() {
		return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	}

	bool SameName(const std::string& a, const std::string& b) {
		return !a.empty() && GeneralUtils::CaseInsensitiveStringCompare(a, b);
	}

	// The account and character slots a connection's owner belongs to
	uint8_t MaskFor(uint32_t accountId, const std::string& accountName, LWOOBJID characterId) {
		uint8_t mask = 0;
		for (uint8_t i = 0; i < g_Slots.size(); i++) {
			const auto& slot = g_Slots[i];
			if (!slot.armed || slot.target == eCaptureTarget::EVERYTHING) continue;
			const bool character = characterId != 0 && std::ranges::find(slot.characterIds, characterId) != slot.characterIds.end();
			const bool account = (accountId != 0 && accountId == slot.accountId) || SameName(accountName, slot.accountName);
			if (slot.target == eCaptureTarget::CHARACTER ? character : (account || character)) mask |= 1 << i;
		}
		return mask;
	}

	void Remask() {
		g_EverythingMask = g_SubjectMask = g_AccountMask = 0;
		for (uint8_t i = 0; i < g_Slots.size(); i++) {
			const auto& slot = g_Slots[i];
			if (!slot.armed) continue;
			if (slot.target == eCaptureTarget::EVERYTHING) g_EverythingMask |= 1 << i;
			else g_SubjectMask |= 1 << i;
			if (slot.target == eCaptureTarget::ACCOUNT) g_AccountMask |= 1 << i;
		}
		for (auto& [key, binding] : g_Bindings) binding.mask = MaskFor(binding.accountId, binding.accountName, binding.characterId);
	}

	void Seal() {
		if (g_Chunk.count == 0) return;
		for (size_t i = 0; i < g_Slots.size(); i++) g_Chunk.slots[i] = g_Slots[i].armed ? g_Slots[i].captureId : 0;
		g_SealedBytes += g_Chunk.records.size();
		g_Sealed.push_back(std::move(g_Chunk));
		g_Chunk = Chunk{};
		g_Chunk.records.reserve(g_Settings.flushBytes + 4096);
		g_ChunkStarted = Clock::now();
		// Over the cap: the oldest go first, and the dashboard is told how many
		while (g_SealedBytes > g_Settings.maxBufferBytes && g_Sealed.size() > 1) {
			g_SealedBytes -= g_Sealed.front().records.size();
			g_Dropped += g_Sealed.front().count;
			g_Stats.dropped += g_Sealed.front().count;
			g_Sealed.pop_front();
		}
	}

	void Append(const PacketRecordHeader& header, const unsigned char* data) {
		PacketRecord::Append(g_Chunk.records, header, data);
		g_Chunk.count++;
		g_Stats.recorded++;
		g_Stats.recordedBytes += sizeof(header) + header.length;
		if (g_Chunk.records.size() >= g_Settings.flushBytes) Seal();
	}

	PacketRecordHeader Header(ePacketDirection direction, uint8_t flags, uint64_t peer, uint32_t bits) {
		PacketRecordHeader header;
		header.timeUs = NowUs();
		header.seq = ++g_Seq;
		header.source = static_cast<uint8_t>(g_Source);
		header.direction = static_cast<uint8_t>(direction);
		header.flags = flags;
		header.peer = peer;
		header.zoneId = g_Zone;
		header.instanceId = g_Instance;
		header.cloneId = g_Clone;
		header.bits = bits;
		const auto bytes = (bits + 7) / 8;
		header.length = std::min<uint32_t>(bytes, PacketRecord::MAX_BYTES);
		if (header.length < bytes) header.flags |= PacketRecordFlags::CUT;
		return header;
	}

	// An LU packet's service and message ID, if it is one
	bool LuHeader(const unsigned char* data, uint32_t bits, ServiceType& service, uint32_t& id) {
		if (bits < 64 || data[0] != ID_USER_PACKET_ENUM) return false;
		uint16_t rawService;
		std::memcpy(&rawService, data + 1, sizeof(rawService));
		std::memcpy(&id, data + 3, sizeof(id));
		service = static_cast<ServiceType>(rawService);
		return true;
	}

	// Never record the capture's own traffic
	bool IsCaptureTraffic(const unsigned char* data, uint32_t bits) {
		ServiceType service;
		uint32_t id;
		return LuHeader(data, bits, service, id) && service == ServiceType::MASTER &&
			(id == static_cast<uint32_t>(Master::MESSAGE_CAPTURE_CONTROL) || id == static_cast<uint32_t>(Master::MESSAGE_CAPTURE_DATA));
	}

	// Chat: every chat packet between worlds and chat starts with the player's object ID
	LWOOBJID ChatSubject(const unsigned char* data, uint32_t bits) {
		ServiceType service;
		uint32_t id;
		if (!LuHeader(data, bits, service, id) || bits < 128) return 0;
		LWOOBJID subject;
		std::memcpy(&subject, data + 8, sizeof(subject));
		return subject;
	}

	/**
	 * Secrets never reach a record (PacketDecoder::Redact): packets that carry them are rewritten with them blanked,
	 * into `scratch`, and dropped if they don't read. Auth keeps only the handshake and the login request and
	 * response (both redacted). False: don't record it.
	 */
	bool Prepare(const unsigned char*& data, uint32_t& bits, std::string& scratch) {
		ServiceType service;
		uint32_t id;
		if (!LuHeader(data, bits, service, id)) return true;
		if (g_Source == eCaptureSource::AUTH && service != ServiceType::COMMON &&
			!(service == ServiceType::AUTH && id == static_cast<uint32_t>(MessageType::Auth::LOGIN_REQUEST)) &&
			!(service == ServiceType::CLIENT && id == static_cast<uint32_t>(MessageType::Client::LOGIN_RESPONSE))) return false;
		if (!PacketDecoder::HasSecrets(service, id)) return true;
		scratch.assign(reinterpret_cast<const char*>(data), (bits + 7) / 8);
		if (!PacketDecoder::Redact(scratch)) return false;
		data = reinterpret_cast<const unsigned char*>(scratch.data());
		bits = static_cast<uint32_t>(scratch.size() * 8);
		return true;
	}

	// A packet on the listening peer: who it belongs to, and so which captures keep it
	void RecordMain(const SystemAddress& address, ePacketDirection direction, bool broadcast, const unsigned char* data, uint32_t bits) {
		if (bits < 8 || IsCaptureTraffic(data, bits)) return;
		std::string scratch;
		if (!Prepare(data, bits, scratch)) return;
		const auto key = Key(address);
		if (!g_Ignored.empty() && std::ranges::find(g_Ignored, key) != g_Ignored.end()) return;

		// Master: server-to-server traffic belongs to EVERYTHING captures only (the other servers record what belongs to a player)
		if (g_Source == eCaptureSource::MASTER) {
			if (!g_EverythingMask) return;
			auto header = Header(direction, broadcast ? PacketRecordFlags::BROADCAST : 0, key, bits);
			header.mask = g_EverythingMask;
			Append(header, data);
			return;
		}

		uint8_t mask = g_EverythingMask;
		uint32_t accountId = 0;
		LWOOBJID characterId = 0;
		if (g_SubjectMask) {
			if (g_Source == eCaptureSource::CHAT) {
				characterId = ChatSubject(data, bits);
				if (characterId) mask |= MaskFor(0, "", characterId);
			} else if (broadcast) {
				// Everyone but `address`: the captured players it reaches
				for (const auto& [bound, binding] : g_Bindings) {
					if (binding.mask && bound != key) {
						mask |= binding.mask;
						accountId = binding.accountId;
						characterId = binding.characterId;
					}
				}
			} else if (const auto it = g_Bindings.find(key); it != g_Bindings.end()) {
				mask |= it->second.mask;
				accountId = it->second.accountId;
				characterId = it->second.characterId;
			} else if (g_AccountMask && g_Source != eCaptureSource::UNKNOWN) {
				// Not known yet whose it is: keep a few until a login says
				auto& pending = g_Pending[key];
				if (pending.count == 0) pending.since = Clock::now();
				auto header = Header(direction, 0, key, bits);
				if (pending.count < PENDING_RECORDS && pending.records.size() + sizeof(header) + header.length <= PENDING_BYTES) {
					PacketRecord::Append(pending.records, header, data);
					pending.count++;
				}
			}
		}
		if (!mask) return;
		auto header = Header(direction, broadcast ? PacketRecordFlags::BROADCAST : 0, key, bits);
		header.mask = mask;
		header.accountId = accountId;
		header.characterId = characterId;
		Append(header, data);
	}

	// A master link message: which captured players it belongs to
	uint8_t MasterLinkMask(ePacketDirection direction, const unsigned char* data, uint32_t bits, uint32_t& accountId, LWOOBJID& characterId) {
		ServiceType service;
		uint32_t id;
		if (!LuHeader(data, bits, service, id) || service != ServiceType::MASTER) return 0;
		RakNet::BitStream stream(const_cast<unsigned char*>(data), (bits + 7) / 8, false);
		LUBitStream header;
		if (!header.ReadHeader(stream)) return 0;

		const auto scope = g_Bindings.find(g_Scope);
		const auto* scoped = g_Scope && scope != g_Bindings.end() && scope->second.mask ? &scope->second : nullptr;
		const auto byName = [&](const std::string& name) { return MaskFor(0, name, 0); };
		uint8_t everyone = 0;
		for (const auto& [key, binding] : g_Bindings) everyone |= binding.mask;

		switch (static_cast<Master>(id)) {
		case Master::MESSAGE_CAPTURE_CONTROL:
		case Master::MESSAGE_CAPTURE_DATA:
		case Master::PLAYER_POSITIONS:
		case Master::DATA_CHANGED:
		case Master::PLAYER_ACTION_RESULT:
			return 0;
		case Master::REQUEST_SESSION_KEY: {
			MasterPackets::RequestSessionKey msg;
			return msg.Deserialize(stream) ? byName(msg.username.GetAsString()) : 0;
		}
		case Master::SESSION_KEY_RESPONSE: {
			MasterPackets::SessionKeyResponse msg;
			return msg.Deserialize(stream) ? byName(msg.username.GetAsString()) : 0;
		}
		case Master::SET_SESSION_KEY: {
			MasterPackets::SetSessionKey msg;
			return msg.Deserialize(stream) ? byName(msg.username.string) : 0;
		}
		case Master::NEW_SESSION_ALERT: {
			MasterPackets::NewSessionAlert msg;
			return msg.Deserialize(stream) ? byName(msg.username.string) : 0;
		}
		case Master::REQUEST_ZONE_TRANSFER: {
			MasterPackets::RequestZoneTransfer msg;
			if (!scoped || !msg.Deserialize(stream)) return 0;
			if (g_Requests.size() >= MAX_REQUESTS) g_Requests.erase(g_Requests.begin());
			g_Requests[msg.requestID] = scoped->mask;
			accountId = scoped->accountId;
			characterId = scoped->characterId;
			return scoped->mask;
		}
		case Master::REQUEST_ZONE_TRANSFER_RESPONSE: {
			MasterPackets::RequestZoneTransferResponse msg;
			if (!msg.Deserialize(stream)) return 0;
			const auto it = g_Requests.find(msg.requestID);
			if (it == g_Requests.end()) return 0;
			const auto mask = it->second & (g_SubjectMask);
			g_Requests.erase(it);
			return mask;
		}
		// Instance-wide: they concern every captured player in this world
		case Master::INSTANCE_MIGRATE:
		case Master::MIGRATE_PLAYERS:
		case Master::MIGRATE_STATUS:
		case Master::MIGRATE_PLAYER_STATE:
		case Master::SHUTDOWN:
		case Master::AFFIRM_TRANSFER_REQUEST:
		case Master::AFFIRM_TRANSFER_RESPONSE:
			return everyone;
		default:
			// Anything else sent while handling a captured player's packet (player added or removed, ...)
			if (direction == ePacketDirection::SENT && scoped) {
				accountId = scoped->accountId;
				characterId = scoped->characterId;
				return scoped->mask;
			}
			return 0;
		}
	}

	void RecordMasterLink(ePacketDirection direction, const unsigned char* data, uint32_t bits) {
		if (!g_SubjectMask || bits < 64) return;
		uint32_t accountId = 0;
		LWOOBJID characterId = 0;
		const auto mask = MasterLinkMask(direction, data, bits, accountId, characterId);
		if (!mask) return;
		std::string scratch;
		if (!Prepare(data, bits, scratch)) return;
		auto header = Header(direction, PacketRecordFlags::MASTER_LINK, 0, bits);
		header.mask = mask;
		header.accountId = accountId;
		header.characterId = characterId;
		Append(header, data);
	}

	void OnSend(RakPeerInterface* peer, const unsigned char* data, BitSize_t bits, SystemAddress address, bool broadcast) {
		if (!PacketCapture::g_Armed || !data) return;
		if (peer == g_Peer) RecordMain(address, ePacketDirection::SENT, broadcast, data, static_cast<uint32_t>(bits));
		else if (peer == g_MasterLink && g_MasterLink) RecordMasterLink(ePacketDirection::SENT, data, static_cast<uint32_t>(bits));
	}

	void SetArmed(bool armed) {
		PacketCapture::g_Armed = armed;
		g_RakPeerSendHook = armed ? &OnSend : nullptr;
		if (!armed) {
			g_Pending.clear();
			g_Requests.clear();
		}
	}

	void ReadSettings() {
		if (!Game::config) return;
		PacketCapture::Settings settings;
		settings.flushIntervalMs = std::clamp<uint32_t>(GeneralUtils::TryParse<uint32_t>(Game::config->GetValue("capture_flush_interval_ms")).value_or(1000), 50, 60000);
		settings.flushBytes = std::clamp<uint32_t>(GeneralUtils::TryParse<uint32_t>(Game::config->GetValue("capture_flush_bytes")).value_or(256 * 1024), 4096, 4 * 1024 * 1024);
		settings.maxBufferBytes = std::clamp<uint64_t>(GeneralUtils::TryParse<uint64_t>(Game::config->GetValue("capture_buffer_max_mb")).value_or(16), 1, 1024) * 1024 * 1024;
		PacketCapture::SetSettings(settings);
	}

	bool SendToMaster(MessageCaptureData& data) {
		if (!Game::server || !Game::server->GetIsConnectedToMaster()) return false;
		MasterPackets::SendToMaster(data);
		return true;
	}
}

namespace PacketCapture {
	bool g_Armed = false;
	bool g_Tracking = false;

	void Attach(ServiceType serverType, RakPeerInterface* peer, RakPeerInterface* masterLink, uint32_t zoneId, uint32_t instanceId) {
		g_ServerType = serverType;
		switch (serverType) {
		case ServiceType::AUTH: g_Source = eCaptureSource::AUTH; break;
		case ServiceType::CHAT: g_Source = eCaptureSource::CHAT; break;
		case ServiceType::WORLD: g_Source = eCaptureSource::WORLD; break;
		case ServiceType::MASTER: g_Source = eCaptureSource::MASTER; break;
		default: g_Source = eCaptureSource::UNKNOWN; break;
		}
		g_Peer = peer;
		g_MasterLink = serverType == ServiceType::MASTER ? nullptr : masterLink;
		g_Zone = static_cast<uint16_t>(zoneId);
		g_Instance = static_cast<uint16_t>(instanceId);
	}

	void Detach() {
		Reset();
		g_Peer = g_MasterLink = nullptr;
	}

	void SetClone(uint32_t cloneId) { g_Clone = cloneId; }

	void IgnorePeer(const SystemAddress& address) {
		const auto key = Key(address);
		if (std::ranges::find(g_Ignored, key) == g_Ignored.end()) g_Ignored.push_back(key);
	}

	void SetSink(Sink sink) { g_Sink = std::move(sink); }

	void SetSettings(const Settings& settings) {
		g_Settings = settings;
		if (g_Chunk.records.capacity() < g_Settings.flushBytes) g_Chunk.records.reserve(g_Settings.flushBytes + 4096);
	}

	const Stats& GetStats() { return g_Stats; }

	void Control(const MessageCaptureControl& control) {
		if (control.action != eMessageCaptureControl::ARM && control.action != eMessageCaptureControl::DISARM) return;
		if (control.slot >= g_Slots.size()) return;
		auto& slot = g_Slots[control.slot];
		const bool sameCapture = slot.armed && slot.captureId == control.captureId;
		if (control.action == eMessageCaptureControl::DISARM) {
			if (!sameCapture) return;
			Seal();
			slot = Slot{};
			CAPTURE_LOG("Packet capture %u ended here", control.captureId);
		} else {
			if (control.seconds == 0) return;
			// The slot changes hands: what was recorded so far goes out under the old capture
			if (!sameCapture) Seal();
			if (!sameCapture) CAPTURE_LOG("Packet capture %u armed here (%s) for %u seconds", control.captureId,
				control.target == eCaptureTarget::EVERYTHING ? "everything" : control.target == eCaptureTarget::ACCOUNT ? "an account" : "a character", control.seconds);
			slot.armed = true;
			slot.captureId = control.captureId;
			slot.target = control.target;
			slot.accountId = control.accountId;
			slot.accountName = control.accountName;
			slot.characterIds = control.characterIds;
			slot.until = Clock::now() + std::chrono::seconds(std::min(control.seconds, MessageCapture::MAX_SECONDS));
			ReadSettings();
			if (g_ChunkStarted == Clock::time_point{}) g_ChunkStarted = Clock::now();
		}
		Remask();
		// Stays armed while chunks wait to be sent, so Update keeps sending them
		SetArmed(std::ranges::any_of(g_Slots, &Slot::armed) || !g_Sealed.empty() || g_Chunk.count);
	}

	void OnReceiveImpl(const Packet* packet) {
		// Closed connections are forgotten once their disconnect was handled (a world still names the player then)
		if (!g_Unbind.empty()) {
			for (const auto key : g_Unbind) {
				g_Bindings.erase(key);
				g_Pending.erase(key);
			}
			g_Unbind.clear();
			g_Tracking = !g_Bindings.empty();
		}
		if (!packet) {
			g_Scope = 0;
			return;
		}
		g_Scope = Key(packet->systemAddress);
		if (packet->length >= 1 && (packet->data[0] == ID_DISCONNECTION_NOTIFICATION || packet->data[0] == ID_CONNECTION_LOST)) g_Unbind.push_back(g_Scope);
		if (!g_Armed) return;
		RecordMain(packet->systemAddress, ePacketDirection::RECEIVED, false, packet->data, static_cast<uint32_t>(packet->bitSize));
	}

	void OnReceiveFromMasterImpl(const Packet* packet) {
		if (!packet || !g_MasterLink) return;
		RecordMasterLink(ePacketDirection::RECEIVED, packet->data, static_cast<uint32_t>(packet->bitSize));
	}

	void Bind(const SystemAddress& address, uint32_t accountId, const std::string& accountName) {
		const auto key = Key(address);
		auto& binding = g_Bindings[key];
		g_Tracking = true;
		binding.accountId = accountId;
		binding.accountName = accountName;
		binding.mask = MaskFor(accountId, accountName, binding.characterId);

		// What came before the login, now that it is known to be a captured account's
		const auto pending = g_Pending.find(key);
		if (pending == g_Pending.end()) return;
		const auto mask = binding.mask & g_AccountMask;
		if (mask) {
			PacketRecord::ForEach(pending->second.records, [&](PacketRecordHeader header, std::string_view bytes) {
				header.mask = mask;
				header.accountId = accountId;
				Append(header, reinterpret_cast<const unsigned char*>(bytes.data()));
			});
		}
		g_Pending.erase(pending);
	}

	void BindCharacter(const SystemAddress& address, LWOOBJID characterId) {
		auto& binding = g_Bindings[Key(address)];
		g_Tracking = true;
		binding.characterId = characterId;
		binding.mask = MaskFor(binding.accountId, binding.accountName, characterId);
	}

	void Update() {
		if (!g_Armed) return;
		const auto now = Clock::now();

		// Captures past their time end here on their own
		bool changed = false;
		for (auto& slot : g_Slots) {
			if (slot.armed && now >= slot.until) {
				Seal();
				CAPTURE_LOG("Packet capture %u reached its time limit here", slot.captureId);
				slot = Slot{};
				changed = true;
			}
		}
		if (changed) Remask();

		std::erase_if(g_Pending, [now](const auto& entry) { return now - entry.second.since > PENDING_FOR; });

		if (g_Chunk.count && (g_Chunk.records.size() >= g_Settings.flushBytes || now - g_ChunkStarted >= std::chrono::milliseconds(g_Settings.flushIntervalMs))) Seal();
		if (g_Chunk.count == 0) g_ChunkStarted = now;

		while (!g_Sealed.empty() || g_Dropped) {
			MessageCaptureData data;
			data.status = eMessageCaptureStatus::PACKETS;
			data.source = static_cast<uint8_t>(g_Source);
			data.zoneId = g_Zone;
			data.instanceId = g_Instance;
			data.cloneId = g_Clone;
			data.packetsDropped = g_Dropped;
			if (!g_Sealed.empty()) {
				auto& chunk = g_Sealed.front();
				data.slots = chunk.slots;
				data.packetCount = chunk.count;
				data.packets = std::move(chunk.records);
			} else {
				for (size_t i = 0; i < g_Slots.size(); i++) data.slots[i] = g_Slots[i].armed ? g_Slots[i].captureId : 0;
			}
			const bool sent = g_Sink ? g_Sink(data) : SendToMaster(data);
			if (!sent) {
				// Put it back and try again next time
				if (!g_Sealed.empty()) g_Sealed.front().records = std::move(data.packets);
				break;
			}
			g_Stats.batches++;
			g_Stats.sentBytes += data.packets.size();
			g_Dropped = 0;
			if (!g_Sealed.empty()) {
				g_SealedBytes -= std::min<uint64_t>(g_SealedBytes, data.packets.size());
				g_Sealed.pop_front();
			}
		}

		if (!std::ranges::any_of(g_Slots, &Slot::armed) && g_Sealed.empty() && g_Chunk.count == 0) SetArmed(false);
	}

	void Reset() {
		g_Slots = {};
		g_Bindings.clear();
		g_Tracking = false;
		g_Unbind.clear();
		g_Pending.clear();
		g_Requests.clear();
		g_Ignored.clear();
		g_Scope = 0;
		g_Chunk = Chunk{};
		g_Sealed.clear();
		g_SealedBytes = 0;
		g_Dropped = 0;
		g_Seq = 0;
		g_Stats = {};
		g_Sink = nullptr;
		g_Settings = {};
		Remask();
		SetArmed(false);
	}

	void RecordForTest(const SystemAddress& address, bool sent, bool broadcast, const unsigned char* data, uint32_t bits) {
		if (!g_Armed) return;
		RecordMain(address, sent ? ePacketDirection::SENT : ePacketDirection::RECEIVED, broadcast, data, bits);
	}
}
