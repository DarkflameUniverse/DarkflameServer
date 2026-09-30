#include "PacketDecoder.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <deque>
#include <set>
#include <unordered_map>
#include <map>
#include <utility>

#include "AuthPackets.h"
#include "BitStreamUtils.h"
#include "ChatPackets.h"
#include "ClientPackets.h"
#include "CommonPackets.h"
#include "MasterPackets.h"
#include "MessageIdentifiers.h"
#include "MessageType/Auth.h"
#include "MessageType/Chat.h"
#include "MessageType/Client.h"
#include "MessageType/Master.h"
#include "MessageType/Server.h"
#include "MessageType/World.h"
#include "ServiceType.h"
#include "WorldPackets.h"
#include "WorldRoutePacket.h"
#include "magic_enum.hpp"
#include "PacketJson.h"
#include "master/CDClientReload.h"
#include "master/DashboardMessages.h"
#include "master/DataChanged.h"
#include "master/InstanceMigration.h"
#include "master/LiveUpdate.h"
#include "master/MessageCapture.h"
#include "master/PlayerAction.h"
#include "master/Profiling.h"
#include "master/ServerTraffic.h"
#include "master/UgcModelsMade.h"
#include "master/UpdateStatus.h"
#include "master/WorldFiles.h"
#include "Profiler.h"
#include "TrafficStats.h"
#include "ZoneFileLog.h"

namespace {
	using json = nlohmann::json;
	using Redactor = std::function<bool(std::string&)>;

	using Scrubber = std::function<bool(std::string&, bool anonymise)>;

	// Packets with secrets or text to scrub (the fields of every packet come from PacketFields.inc)
	struct Protection {
		Redactor redact; // set for structs with secret fields
		Scrubber scrub;  // set for structs with account names, character names or typed text
	};

	void X(std::u16string& text) { for (auto& c : text) c = u'x'; }
	void X(std::string& text) { for (auto& c : text) c = 'x'; }
	void X(LUWString& text) { X(text.string); }
	void X(LUString& text) { X(text.string); }

	// Like Blank, for Scrub: `names(packet)` replaces account names, `anonymous(packet)` the rest
	template<typename T>
	Scrubber Scrub(std::function<void(T&)> names, std::function<void(T&)> anonymous = nullptr) {
		return [names, anonymous](std::string& bytes, bool anonymise) {
			RakNet::BitStream in(reinterpret_cast<unsigned char*>(bytes.data()), static_cast<unsigned int>(bytes.size()), false);
			T packet;
			if (!packet.ReadHeader(in) || !packet.Deserialize(in)) return false;
			if (names) names(packet);
			if (anonymise && anonymous) anonymous(packet);
			RakNet::BitStream out;
			packet.WritePacket(out);
			std::string written(reinterpret_cast<const char*>(out.GetData()), out.GetNumberOfBytesUsed());
			if (written == bytes) return false;
			bytes = std::move(written);
			return true;
		};
	}

	PacketDecoder::GameMessageFields g_GameMessages;

	using Rewriter = std::function<std::optional<std::string>(std::string_view)>;
	template<typename T>
	Rewriter Rewrite() {
		return [](std::string_view bytes) -> std::optional<std::string> {
			RakNet::BitStream in(reinterpret_cast<unsigned char*>(const_cast<char*>(bytes.data())), static_cast<unsigned int>(bytes.size()), false);
			T packet;
			if (!packet.ReadHeader(in) || !packet.Deserialize(in)) return std::nullopt;
			RakNet::BitStream out;
			packet.WritePacket(out);
			return std::string(reinterpret_cast<const char*>(out.GetData()), out.GetNumberOfBytesUsed());
		};
	}

	// The fields of every packet: its struct's own Deserialize, then its members by name (PacketFields.inc, written by
	// tools/gen_game_message_fields.py from the structs)
	using PacketJson::ToJson;

	json ToJson(const TrafficStats::Histogram& histogram) {
		json buckets = json::array();
		for (const auto& [bucket, count] : histogram.Sparse()) buckets.push_back(json::array({ bucket, count }));
		return json{ {"count", std::to_string(histogram.Count())}, {"sumUs", std::to_string(histogram.Sum())}, {"buckets", buckets} };
	}

	template<typename T> json ToJson(const std::optional<T>& value);
	template<typename T> json ToJson(const std::vector<T>& values);
	template<typename K, typename V> json ToJson(const std::map<K, V>& values);
	template<typename A, typename B> json ToJson(const std::pair<A, B>& value);
	template<typename T, size_t N> json ToJson(const std::array<T, N>& values);
	template<typename T> json ToJson(const std::set<T>& values);
	template<typename T> json ToJson(const std::deque<T>& values);
	template<typename K, typename V> json ToJson(const std::unordered_map<K, V>& values);

	struct Read {
		std::optional<json> fields;
		uint32_t unreadBits{};
	};
	using Reader = Read(*)(RakNet::BitStream&);

	struct Entry {
		ServiceType service;
		uint32_t id;
		const char* structName;
		bool received; // read by the server from its client (has a Handle)
		Reader read;
	};

	template<typename T> Read ReadWith(RakNet::BitStream& stream);

#include "PacketFields.inc"

	template<typename T> json ToJson(const std::optional<T>& value) { return value ? ToJson(*value) : json(nullptr); }
	template<typename T> json ToJson(const std::vector<T>& values) {
		if constexpr (std::is_same_v<T, uint8_t> || std::is_same_v<T, char>) {
			return json{ {"hex", PacketJson::Hex(std::string_view(reinterpret_cast<const char*>(values.data()), values.size()))} };
		} else {
			json out = json::array();
			for (const auto& value : values) out.push_back(ToJson(value));
			return out;
		}
	}
	template<typename K, typename V> json ToJson(const std::map<K, V>& values) {
		json out = json::array();
		for (const auto& [key, value] : values) out.push_back(json::array({ ToJson(key), ToJson(value) }));
		return out;
	}
	template<typename A, typename B> json ToJson(const std::pair<A, B>& value) { return json::array({ ToJson(value.first), ToJson(value.second) }); }
	template<typename T, size_t N> json ToJson(const std::array<T, N>& values) {
		json out = json::array();
		for (const auto& value : values) out.push_back(ToJson(value));
		return out;
	}

	template<typename T> json ToJson(const std::set<T>& values) {
		json out = json::array();
		for (const auto& value : values) out.push_back(ToJson(value));
		return out;
	}
	template<typename T> json ToJson(const std::deque<T>& values) {
		json out = json::array();
		for (const auto& value : values) out.push_back(ToJson(value));
		return out;
	}
	template<typename K, typename V> json ToJson(const std::unordered_map<K, V>& values) {
		json out = json::array();
		for (const auto& [key, value] : values) out.push_back(json::array({ ToJson(key), ToJson(value) }));
		return out;
	}

	template<typename T> Read ReadWith(RakNet::BitStream& stream) {
		T packet;
		if (!packet.Deserialize(stream)) return {};
		return { ToJson(packet), stream.GetNumberOfUnreadBits() };
	}

	// (service, ID) -> its structs, the ones the server reads from a client first
	const std::map<std::pair<ServiceType, uint32_t>, std::vector<const Entry*>>& FieldIndex() {
		static const auto index = [] {
			std::map<std::pair<ServiceType, uint32_t>, std::vector<const Entry*>> out;
			for (const auto& entry : Entries()) out[{ entry.service, entry.id }].push_back(&entry);
			return out;
		}();
		return index;
	}

	/**
	 * The packet's fields from the first of its structs that reads the whole of it (a struct per direction can share
	 * an ID: the one for this direction is tried first). A struct that reads but leaves whole bytes unread is shown
	 * with "(unread bits)" when no struct reads it all.
	 */
	std::optional<json> ReadFields(ServiceType service, uint32_t id, bool fromClient, RakNet::BitStream& stream, bool& failed) {
		const auto it = FieldIndex().find({ service, id });
		if (it == FieldIndex().end()) return std::nullopt;
		auto candidates = it->second;
		std::stable_sort(candidates.begin(), candidates.end(), [fromClient](const Entry* a, const Entry* b) {
			return (a->received == fromClient) > (b->received == fromClient);
		});
		const auto start = stream.GetReadOffset();
		std::optional<json> partial;
		for (const auto* entry : candidates) {
			if (!entry->read) continue;
			stream.SetReadOffset(start);
			auto read = entry->read(stream);
			if (!read.fields) continue;
			if (read.unreadBits < 8) return read.fields;
			if (!partial) {
				partial = std::move(read.fields);
				(*partial)["(unread bits)"] = read.unreadBits;
			}
		}
		failed = !partial;
		return partial;
	}

	// Reads T from a whole packet, lets `blank` clear its secrets, and writes it back in place
	template<typename T>
	Redactor Blank(std::function<void(T&)> blank) {
		return [blank](std::string& bytes) {
			RakNet::BitStream in(reinterpret_cast<unsigned char*>(bytes.data()), static_cast<unsigned int>(bytes.size()), false);
			T packet;
			if (!packet.ReadHeader(in) || !packet.Deserialize(in)) return false;
			blank(packet);
			RakNet::BitStream out;
			packet.WritePacket(out);
			bytes.assign(reinterpret_cast<const char*>(out.GetData()), out.GetNumberOfBytesUsed());
			return true;
		};
	}

	using Key = std::pair<ServiceType, uint32_t>;
	template<typename E> Key K(ServiceType service, E id) { return { service, static_cast<uint32_t>(id) }; }

	const std::map<Key, Protection>& Registry() {
		using S = ServiceType;
		static const std::map<Key, Protection> registry{
			{ K(S::AUTH, MessageType::Auth::LOGIN_REQUEST), { Blank<AuthPackets::LoginRequest>([](auto& p) { p.username.string.clear(); p.password.string.clear(); }), Scrub<AuthPackets::LoginRequest>([](auto& p) { X(p.username); }) } },
			{ K(S::CLIENT, MessageType::Client::LOGIN_RESPONSE), { Blank<ClientPackets::LoginResponse>([](auto& p) { p.userKey.string.clear(); p.cdnKey.string.clear(); }), nullptr } },
			{ K(S::WORLD, MessageType::World::VALIDATION), { Blank<WorldPackets::Validation>([](auto& p) { p.sessionKey.string.clear(); }), Scrub<WorldPackets::Validation>([](auto& p) { X(p.username); }) } },
			{ K(S::WORLD, MessageType::World::CHARACTER_CREATE_REQUEST), { nullptr, Scrub<WorldPackets::CharacterCreateRequest>(nullptr, [](auto& p) { X(p.name); }) } },
			{ K(S::WORLD, MessageType::World::CHARACTER_RENAME_REQUEST), { nullptr, Scrub<WorldPackets::CharacterRenameRequest>(nullptr, [](auto& p) { X(p.name); }) } },
			{ K(S::WORLD, MessageType::World::GENERAL_CHAT_MESSAGE), { nullptr, Scrub<WorldPackets::GeneralChatMessage>(nullptr, [](auto& p) { X(p.message); }) } },
			{ K(S::CLIENT, MessageType::Client::CHARACTER_LIST_RESPONSE), { nullptr, Scrub<ClientPackets::CharacterListResponse>(nullptr, [](auto& p) { for (auto& c : p.characters) { X(c.name); X(c.unapprovedName); } }) } },
			{ K(S::CLIENT, MessageType::Client::CREATE_CHARACTER), { nullptr, Scrub<ClientPackets::CreateCharacter>(nullptr, [](auto& p) { X(p.name); }) } },
			{ K(S::CHAT, MessageType::Chat::GENERAL_CHAT_MESSAGE), { nullptr, Scrub<ChatPackets::GeneralChatMessage>(nullptr, [](auto& p) { X(p.senderName); X(p.message); }) } },
			{ K(S::CHAT, MessageType::Chat::PRIVATE_CHAT_MESSAGE), { nullptr, Scrub<ChatPackets::PrivateChatMessage>(nullptr, [](auto& p) { X(p.senderName); X(p.receiverName); X(p.message); }) } },
			{ K(S::MASTER, MessageType::Master::REQUEST_SESSION_KEY), { nullptr, Scrub<MasterPackets::RequestSessionKey>([](auto& p) { X(p.username); }) } },
			{ K(S::MASTER, MessageType::Master::SESSION_KEY_RESPONSE), { Blank<MasterPackets::SessionKeyResponse>([](auto& p) { p.sessionKey = 0; }), Scrub<MasterPackets::SessionKeyResponse>([](auto& p) { X(p.username); }) } },
			{ K(S::MASTER, MessageType::Master::SET_SESSION_KEY), { Blank<MasterPackets::SetSessionKey>([](auto& p) { p.sessionKey = 0; }), Scrub<MasterPackets::SetSessionKey>([](auto& p) { X(p.username); }) } },
			{ K(S::MASTER, MessageType::Master::NEW_SESSION_ALERT), { Blank<MasterPackets::NewSessionAlert>([](auto& p) { p.sessionKey = 0; }), Scrub<MasterPackets::NewSessionAlert>([](auto& p) { X(p.username); }) } },
		};
		return registry;
	}

	const std::map<Key, Rewriter>& Rewriters() {
		using S = ServiceType;
		static const std::map<Key, Rewriter> rewriters{
			{ K(S::AUTH, MessageType::Auth::LOGIN_REQUEST), Rewrite<AuthPackets::LoginRequest>() },
			{ K(S::CLIENT, MessageType::Client::LOGIN_RESPONSE), Rewrite<ClientPackets::LoginResponse>() },
			{ K(S::WORLD, MessageType::World::VALIDATION), Rewrite<WorldPackets::Validation>() },
			{ K(S::WORLD, MessageType::World::CHARACTER_CREATE_REQUEST), Rewrite<WorldPackets::CharacterCreateRequest>() },
			{ K(S::WORLD, MessageType::World::LOGIN_REQUEST), Rewrite<WorldPackets::CharacterLoginRequest>() },
			{ K(S::WORLD, MessageType::World::CHARACTER_DELETE_REQUEST), Rewrite<WorldPackets::CharacterDeleteRequest>() },
			{ K(S::WORLD, MessageType::World::CHARACTER_RENAME_REQUEST), Rewrite<WorldPackets::CharacterRenameRequest>() },
			{ K(S::WORLD, MessageType::World::LEVEL_LOAD_COMPLETE), Rewrite<WorldPackets::LevelLoadComplete>() },
			{ K(S::WORLD, MessageType::World::POSITION_UPDATE), Rewrite<WorldPackets::PositionUpdate>() },
			{ K(S::WORLD, MessageType::World::GENERAL_CHAT_MESSAGE), Rewrite<WorldPackets::GeneralChatMessage>() },
			{ K(S::WORLD, MessageType::World::ROUTE_PACKET), Rewrite<WorldPackets::RoutePacket>() },
			{ K(S::CLIENT, MessageType::Client::LOAD_STATIC_ZONE), Rewrite<ClientPackets::LoadStaticZone>() },
			{ K(S::CLIENT, MessageType::Client::CHARACTER_LIST_RESPONSE), Rewrite<ClientPackets::CharacterListResponse>() },
			{ K(S::CLIENT, MessageType::Client::CHARACTER_CREATE_RESPONSE), Rewrite<ClientPackets::CharacterCreateResponse>() },
			{ K(S::CLIENT, MessageType::Client::CREATE_CHARACTER), Rewrite<ClientPackets::CreateCharacter>() },
			{ K(S::CLIENT, MessageType::Client::TRANSFER_TO_WORLD), Rewrite<ClientPackets::TransferToWorld>() },
			{ K(S::CHAT, MessageType::Chat::GENERAL_CHAT_MESSAGE), Rewrite<ChatPackets::GeneralChatMessage>() },
			{ K(S::CHAT, MessageType::Chat::PRIVATE_CHAT_MESSAGE), Rewrite<ChatPackets::PrivateChatMessage>() },
			{ K(S::CHAT, MessageType::Chat::WORLD_ROUTE_PACKET), Rewrite<ChatPackets::WorldRoutePacket>() },
			{ K(S::MASTER, MessageType::Master::REQUEST_SESSION_KEY), Rewrite<MasterPackets::RequestSessionKey>() },
			{ K(S::MASTER, MessageType::Master::SESSION_KEY_RESPONSE), Rewrite<MasterPackets::SessionKeyResponse>() },
			{ K(S::MASTER, MessageType::Master::SET_SESSION_KEY), Rewrite<MasterPackets::SetSessionKey>() },
			{ K(S::MASTER, MessageType::Master::NEW_SESSION_ALERT), Rewrite<MasterPackets::NewSessionAlert>() },
			{ K(S::MASTER, MessageType::Master::REQUEST_ZONE_TRANSFER), Rewrite<MasterPackets::RequestZoneTransfer>() },
			{ K(S::MASTER, MessageType::Master::REQUEST_ZONE_TRANSFER_RESPONSE), Rewrite<MasterPackets::RequestZoneTransferResponse>() },
			{ K(S::MASTER, MessageType::Master::PLAYER_ADDED), Rewrite<MasterPackets::PlayerAdded>() },
			{ K(S::MASTER, MessageType::Master::PLAYER_REMOVED), Rewrite<MasterPackets::PlayerRemoved>() },
		};
		return rewriters;
	}

	template<typename E>
	std::string EnumName(uint32_t id) {
		const auto name = magic_enum::enum_name(static_cast<E>(id));
		return name.empty() ? std::to_string(id) : std::string(name);
	}

	std::string RakNetName(uint8_t id) {
		switch (id) {
		case ID_CONNECTION_REQUEST_ACCEPTED: return "ID_CONNECTION_REQUEST_ACCEPTED";
		case ID_NEW_INCOMING_CONNECTION: return "ID_NEW_INCOMING_CONNECTION";
		case ID_DISCONNECTION_NOTIFICATION: return "ID_DISCONNECTION_NOTIFICATION";
		case ID_CONNECTION_LOST: return "ID_CONNECTION_LOST";
		case ID_TIMESTAMP: return "ID_TIMESTAMP";
		case ID_REPLICA_MANAGER_CONSTRUCTION: return "ID_REPLICA_MANAGER_CONSTRUCTION";
		case ID_REPLICA_MANAGER_SCOPE_CHANGE: return "ID_REPLICA_MANAGER_SCOPE_CHANGE";
		case ID_REPLICA_MANAGER_SERIALIZE: return "ID_REPLICA_MANAGER_SERIALIZE";
		case ID_REPLICA_MANAGER_DESTRUCTION: return "ID_REPLICA_MANAGER_DESTRUCTION";
		case ID_REPLICA_MANAGER_DOWNLOAD_STARTED: return "ID_REPLICA_MANAGER_DOWNLOAD_STARTED";
		case ID_REPLICA_MANAGER_DOWNLOAD_COMPLETE: return "ID_REPLICA_MANAGER_DOWNLOAD_COMPLETE";
		default: return "RAKNET_" + std::to_string(id);
		}
	}

	bool ReadLuHeader(std::string_view bytes, ServiceType& service, uint32_t& id) {
		if (bytes.size() < 8 || static_cast<uint8_t>(bytes[0]) != ID_USER_PACKET_ENUM) return false;
		uint16_t raw;
		std::memcpy(&raw, bytes.data() + 1, sizeof(raw));
		std::memcpy(&id, bytes.data() + 3, sizeof(id));
		service = static_cast<ServiceType>(raw);
		return true;
	}
}

namespace {
	// Routed packets: the name of the packet they carry
	void AddRoutedName(ServiceType service, uint32_t id, std::string_view bytes, json& fields) {
		RakNet::BitStream stream(reinterpret_cast<unsigned char*>(const_cast<char*>(bytes.data())), static_cast<unsigned int>(bytes.size()), false);
		stream.IgnoreBytes(8);
		if (service == ServiceType::WORLD && id == static_cast<uint32_t>(MessageType::World::ROUTE_PACKET)) {
			WorldPackets::RoutePacket packet;
			if (packet.Deserialize(stream)) fields["routed"] = PacketDecoder::Name(packet.routedService, packet.routedMessageID);
		} else if (service == ServiceType::CHAT && id == static_cast<uint32_t>(MessageType::Chat::WORLD_ROUTE_PACKET)) {
			ChatPackets::WorldRoutePacket packet;
			if (packet.Deserialize(stream) && packet.routedData.size() >= 8 && packet.routedData[0] == ID_USER_PACKET_ENUM) {
				uint16_t routedService;
				uint32_t routedId;
				std::memcpy(&routedService, packet.routedData.data() + 1, sizeof(routedService));
				std::memcpy(&routedId, packet.routedData.data() + 3, sizeof(routedId));
				fields["routed"] = PacketDecoder::Name(static_cast<ServiceType>(routedService), routedId);
			}
		}
	}
}

namespace PacketDecoder {
	std::string Name(ServiceType service, uint32_t messageId) {
		switch (service) {
		case ServiceType::COMMON: return EnumName<MessageType::Server>(messageId);
		case ServiceType::AUTH: return EnumName<MessageType::Auth>(messageId);
		case ServiceType::CHAT: return EnumName<MessageType::Chat>(messageId);
		case ServiceType::WORLD: return EnumName<MessageType::World>(messageId);
		case ServiceType::CLIENT: return EnumName<MessageType::Client>(messageId);
		case ServiceType::MASTER: return EnumName<MessageType::Master>(messageId);
		default: return std::to_string(messageId);
		}
	}

	Decoded Decode(std::string_view bytes, bool fromClient) {
		Decoded out;
		if (bytes.empty()) return out;
		ServiceType service{};
		uint32_t id{};
		if (!ReadLuHeader(bytes, service, id)) {
			out.service = "RAKNET";
			out.messageId = static_cast<uint8_t>(bytes[0]);
			out.name = RakNetName(static_cast<uint8_t>(bytes[0]));
			// A construction starts with the object and its LOT: [ID][bit][u16 network ID][i64 object][i32 LOT]
			if (out.messageId == ID_REPLICA_MANAGER_CONSTRUCTION) {
				RakNet::BitStream stream(reinterpret_cast<unsigned char*>(const_cast<char*>(bytes.data())), static_cast<unsigned int>(bytes.size()), false);
				stream.IgnoreBytes(1);
				bool flag{};
				uint16_t network{};
				LWOOBJID object{};
				int32_t lot{};
				if (stream.Read(flag) && stream.Read(network) && stream.Read(object) && stream.Read(lot)) {
					out.objectId = object;
					out.fields = json{ {"networkID", network}, {"objectID", std::to_string(object)}, {"lot", lot} };
				}
			}
			return out;
		}
		out.lu = true;
		out.serviceId = static_cast<uint16_t>(service);
		out.messageId = id;
		out.service = std::string(magic_enum::enum_name(service));
		if (out.service.empty()) out.service = std::to_string(out.serviceId);
		out.name = Name(service, id);

		RakNet::BitStream stream(reinterpret_cast<unsigned char*>(const_cast<char*>(bytes.data())), static_cast<unsigned int>(bytes.size()), false);
		stream.IgnoreBytes(8);

		// Game messages: the object, the message ID, then its fields
		const bool gameMessage = (service == ServiceType::WORLD && id == static_cast<uint32_t>(MessageType::World::GAME_MSG)) ||
			(service == ServiceType::CLIENT && id == static_cast<uint32_t>(MessageType::Client::GAME_MSG));
		if (gameMessage) {
			uint16_t messageId{};
			if (!stream.Read(out.objectId) || !stream.Read(messageId)) {
				out.failed = true;
				return out;
			}
			out.gameMessageId = messageId;
			const auto name = magic_enum::enum_name(static_cast<MessageType::Game>(messageId));
			out.name += " " + (name.empty() ? std::to_string(messageId) : std::string(name));
			if (g_GameMessages) {
				// What follows the header, as a stream of its own
				const auto offset = stream.GetReadOffset() / 8;
				RakNet::BitStream payload(reinterpret_cast<unsigned char*>(const_cast<char*>(bytes.data())) + offset, static_cast<unsigned int>(bytes.size() - offset), false);
				out.fields = g_GameMessages(static_cast<MessageType::Game>(messageId), service == ServiceType::WORLD, payload);
			}
			return out;
		}

		out.fields = ReadFields(service, id, fromClient, stream, out.failed);
		if (out.fields) AddRoutedName(service, id, bytes, *out.fields);
		return out;
	}

	std::optional<NiPoint3> Position(std::string_view bytes) {
		ServiceType service{};
		uint32_t id{};
		if (!ReadLuHeader(bytes, service, id) || service != ServiceType::WORLD || id != static_cast<uint32_t>(MessageType::World::POSITION_UPDATE)) return std::nullopt;
		RakNet::BitStream stream(reinterpret_cast<unsigned char*>(const_cast<char*>(bytes.data())), static_cast<unsigned int>(bytes.size()), false);
		stream.IgnoreBytes(8);
		WorldPackets::PositionUpdate update;
		if (!update.Deserialize(stream)) return std::nullopt;
		return update.update.position;
	}

	void SetGameMessageDecoder(GameMessageFields decoder) { g_GameMessages = std::move(decoder); }

	bool Redact(std::string& bytes) {
		ServiceType service{};
		uint32_t id{};
		if (!ReadLuHeader(bytes, service, id)) return true;
		const auto it = Registry().find({ service, id });
		if (it == Registry().end() || !it->second.redact) return true;
		return it->second.redact(bytes);
	}

	bool Scrub(std::string& bytes, bool anonymise) {
		ServiceType service{};
		uint32_t id{};
		if (!ReadLuHeader(bytes, service, id)) return false;
		const auto it = Registry().find({ service, id });
		if (it == Registry().end() || !it->second.scrub) return false;
		return it->second.scrub(bytes, anonymise);
	}

	std::optional<bool> RoundTrip(std::string_view bytes) {
		ServiceType service{};
		uint32_t id{};
		if (!ReadLuHeader(bytes, service, id)) return std::nullopt;
		const auto it = Rewriters().find({ service, id });
		if (it == Rewriters().end()) return std::nullopt;
		const auto written = it->second(bytes);
		if (!written) return std::nullopt;
		return *written == bytes;
	}

	bool HasSecrets(ServiceType service, uint32_t messageId) {
		const auto it = Registry().find({ service, messageId });
		return it != Registry().end() && it->second.redact;
	}

	size_t RegisteredCount() { return FieldIndex().size(); }

	bool HasFields(ServiceType service, uint32_t messageId) { return FieldIndex().contains({ service, messageId }); }
}
