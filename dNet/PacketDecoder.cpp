#include "PacketDecoder.h"

#include <cstring>
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

namespace {
	using json = nlohmann::json;
	using Fields = std::function<std::optional<json>(RakNet::BitStream&, bool fromClient)>;
	using Redactor = std::function<bool(std::string&)>;

	using Scrubber = std::function<bool(std::string&, bool anonymise)>;

	struct Entry {
		Fields fields;
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

	std::string Id(LWOOBJID id) { return std::to_string(id); }
	json Point(const NiPoint3& p) { return json::array({ p.x, p.y, p.z }); }
	json Rotation(const NiQuaternion& q) { return json::array({ q.x, q.y, q.z, q.w }); }

	template<typename T>
	Fields Make(std::function<void(const T&, json&)> fill) {
		return [fill](RakNet::BitStream& stream, bool) -> std::optional<json> {
			T packet;
			if (!packet.Deserialize(stream)) return std::nullopt;
			json out = json::object();
			fill(packet, out);
			return out;
		};
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

	const std::map<Key, Entry>& Registry() {
		using S = ServiceType;
		static const std::map<Key, Entry> registry{
			// Handshake: one ID, a struct per direction
			{ K(S::COMMON, MessageType::Server::VERSION_CONFIRM), { [](RakNet::BitStream& stream, bool fromClient) -> std::optional<json> {
				if (fromClient) {
					CommonPackets::ClientVersionConfirm packet;
					if (!packet.Deserialize(stream)) return std::nullopt;
					return json{ {"netVersion", packet.netVersion}, {"serviceType", static_cast<int>(packet.serviceType)}, {"processID", packet.processID}, {"port", packet.port} };
				}
				CommonPackets::ServerVersionConfirm packet;
				if (!packet.Deserialize(stream)) return std::nullopt;
				return json{ {"netVersion", packet.netVersion}, {"serviceType", static_cast<int>(packet.serviceType)} };
			} } },

			// Auth: the username and password are blanked when recorded
			{ K(S::AUTH, MessageType::Auth::LOGIN_REQUEST), {
				Make<AuthPackets::LoginRequest>([](const auto& p, json& j) {
					j = { {"username", p.username.GetAsString()}, {"localeID", static_cast<int>(p.localeID)}, {"clientOS", static_cast<int>(p.clientOS)},
						{"memoryStats", p.memoryStats.GetAsString()}, {"videoCard", p.videoCard.GetAsString()} };
				}),
				Blank<AuthPackets::LoginRequest>([](auto& p) { p.username.string.clear(); p.password.string.clear(); }),
				Scrub<AuthPackets::LoginRequest>([](auto& p) { X(p.username); }) } },
			{ K(S::CLIENT, MessageType::Client::LOGIN_RESPONSE), {
				Make<ClientPackets::LoginResponse>([](const auto& p, json& j) {
					json stamps = json::array();
					for (const auto& stamp : p.stamps.list) stamps.push_back({ {"type", static_cast<int>(stamp.type)}, {"value", stamp.value}, {"timestamp", stamp.timestamp} });
					j = { {"responseCode", static_cast<int>(p.responseCode)}, {"worldServerIP", p.worldServerIP.string}, {"worldServerPort", p.worldServerPort},
						{"errorMessage", p.errorMessage}, {"stamps", stamps} };
				}),
				Blank<ClientPackets::LoginResponse>([](auto& p) { p.userKey.string.clear(); p.cdnKey.string.clear(); }) } },

			// World
			{ K(S::WORLD, MessageType::World::VALIDATION), {
				Make<WorldPackets::Validation>([](const auto& p, json& j) { j = { {"username", p.username.GetAsString()}, {"fdbChecksum", p.fdbChecksum.string} }; }),
				Blank<WorldPackets::Validation>([](auto& p) { p.sessionKey.string.clear(); }),
				Scrub<WorldPackets::Validation>([](auto& p) { X(p.username); }) } },
			{ K(S::WORLD, MessageType::World::CHARACTER_CREATE_REQUEST), { Make<WorldPackets::CharacterCreateRequest>([](const auto& p, json& j) {
				j = { {"name", p.name.GetAsString()}, {"firstNameIndex", p.firstNameIndex}, {"middleNameIndex", p.middleNameIndex}, {"lastNameIndex", p.lastNameIndex},
					{"shirtColor", p.shirtColor}, {"shirtStyle", p.shirtStyle}, {"pantsColor", p.pantsColor}, {"hairStyle", p.hairStyle}, {"hairColor", p.hairColor},
					{"eyebrows", p.eyebrows}, {"eyes", p.eyes}, {"mouth", p.mouth} };
			}),
				nullptr, Scrub<WorldPackets::CharacterCreateRequest>(nullptr, [](auto& p) { X(p.name); }) } },
			{ K(S::WORLD, MessageType::World::LOGIN_REQUEST), { Make<WorldPackets::CharacterLoginRequest>([](const auto& p, json& j) { j = { {"playerID", Id(p.playerID)} }; }) } },
			{ K(S::WORLD, MessageType::World::CHARACTER_DELETE_REQUEST), { Make<WorldPackets::CharacterDeleteRequest>([](const auto& p, json& j) { j = { {"objectID", Id(p.objectID)} }; }) } },
			{ K(S::WORLD, MessageType::World::CHARACTER_RENAME_REQUEST), { Make<WorldPackets::CharacterRenameRequest>([](const auto& p, json& j) {
				j = { {"objectID", Id(p.objectID)}, {"name", p.name.GetAsString()} };
			}),
				nullptr, Scrub<WorldPackets::CharacterRenameRequest>(nullptr, [](auto& p) { X(p.name); }) } },
			{ K(S::WORLD, MessageType::World::LEVEL_LOAD_COMPLETE), { Make<WorldPackets::LevelLoadComplete>([](const auto& p, json& j) {
				j = { {"mapID", p.mapID}, {"instanceID", p.instanceID}, {"cloneID", p.cloneID} };
			}) } },
			{ K(S::WORLD, MessageType::World::POSITION_UPDATE), { Make<WorldPackets::PositionUpdate>([](const auto& p, json& j) {
				j = { {"position", Point(p.update.position)}, {"rotation", Rotation(p.update.rotation)}, {"onGround", p.update.onGround}, {"onRail", p.update.onRail} };
				if (p.hasVelocity) j["velocity"] = Point(p.update.velocity);
				if (p.hasLocalSpaceInfo) j["platform"] = Id(p.update.localSpaceInfo.objectId);
			}) } },
			{ K(S::WORLD, MessageType::World::GENERAL_CHAT_MESSAGE), { Make<WorldPackets::GeneralChatMessage>([](const auto& p, json& j) {
				j = { {"chatChannel", p.chatChannel}, {"message", GeneralUtils::UTF16ToWTF8(p.message)} };
			}),
				nullptr, Scrub<WorldPackets::GeneralChatMessage>(nullptr, [](auto& p) { X(p.message); }) } },
			{ K(S::WORLD, MessageType::World::ROUTE_PACKET), { Make<WorldPackets::RoutePacket>([](const auto& p, json& j) {
				j = { {"routed", PacketDecoder::Name(p.routedService, p.routedMessageID)}, {"size", p.size} };
			}) } },

			// To the client
			{ K(S::CLIENT, MessageType::Client::LOAD_STATIC_ZONE), { Make<ClientPackets::LoadStaticZone>([](const auto& p, json& j) {
				j = { {"mapID", p.mapID}, {"instanceID", p.instanceID}, {"cloneID", p.cloneID}, {"mapChecksum", p.mapChecksum}, {"playerPosition", Point(p.playerPosition)},
					{"instanceType", p.instanceType} };
			}) } },
			{ K(S::CLIENT, MessageType::Client::CHARACTER_LIST_RESPONSE), { Make<ClientPackets::CharacterListResponse>([](const auto& p, json& j) {
				json characters = json::array();
				for (const auto& c : p.characters) {
					characters.push_back({ {"objectID", Id(c.objectID)}, {"name", c.name.GetAsString()}, {"zoneID", c.zoneID}, {"equippedItems", c.equippedItems} });
				}
				j = { {"selectedCharacterIndex", p.selectedCharacterIndex}, {"characters", characters} };
			}),
				nullptr, Scrub<ClientPackets::CharacterListResponse>(nullptr, [](auto& p) { for (auto& c : p.characters) { X(c.name); X(c.unapprovedName); } }) } },
			{ K(S::CLIENT, MessageType::Client::CREATE_CHARACTER), { Make<ClientPackets::CreateCharacter>([](const auto& p, json& j) {
				j = { {"objectID", Id(p.objectID)}, {"templateID", p.templateID}, {"name", GeneralUtils::UTF16ToWTF8(p.name)}, {"gmLevel", static_cast<int>(p.gmLevel)},
					{"reputation", p.reputation}, {"propertyCloneID", p.propertyCloneID}, {"xmlBytes", p.xmlData.size()} };
			}),
				nullptr, Scrub<ClientPackets::CreateCharacter>(nullptr, [](auto& p) { X(p.name); }) } },
			{ K(S::CLIENT, MessageType::Client::CHARACTER_CREATE_RESPONSE), { Make<ClientPackets::CharacterCreateResponse>([](const auto& p, json& j) {
				j = { {"response", static_cast<int>(p.response)} };
			}) } },
			{ K(S::CLIENT, MessageType::Client::TRANSFER_TO_WORLD), { Make<ClientPackets::TransferToWorld>([](const auto& p, json& j) {
				j = { {"serverIP", p.serverIP.string}, {"serverPort", p.serverPort}, {"mythranShift", p.mythranShift} };
			}) } },

			// Chat
			{ K(S::CHAT, MessageType::Chat::GENERAL_CHAT_MESSAGE), { Make<ChatPackets::GeneralChatMessage>([](const auto& p, json& j) {
				j = { {"playerID", Id(p.playerID)}, {"chatChannel", static_cast<int>(p.chatChannel)}, {"message", p.message.GetAsString()} };
			}),
				nullptr, Scrub<ChatPackets::GeneralChatMessage>(nullptr, [](auto& p) { X(p.senderName); X(p.message); }) } },
			{ K(S::CHAT, MessageType::Chat::PRIVATE_CHAT_MESSAGE), { Make<ChatPackets::PrivateChatMessage>([](const auto& p, json& j) {
				j = { {"playerID", Id(p.playerID)}, {"senderName", p.senderName.GetAsString()}, {"receiverName", p.receiverName.GetAsString()},
					{"responseCode", p.responseCode}, {"message", p.message.GetAsString()} };
			}),
				nullptr, Scrub<ChatPackets::PrivateChatMessage>(nullptr, [](auto& p) { X(p.senderName); X(p.receiverName); X(p.message); }) } },
			{ K(S::CHAT, MessageType::Chat::WORLD_ROUTE_PACKET), { Make<ChatPackets::WorldRoutePacket>([](const auto& p, json& j) {
				j = { {"targetID", Id(p.targetID)}, {"bytes", p.routedData.size()} };
				if (p.routedData.size() >= 8 && p.routedData[0] == ID_USER_PACKET_ENUM) {
					uint16_t service;
					uint32_t id;
					std::memcpy(&service, p.routedData.data() + 1, sizeof(service));
					std::memcpy(&id, p.routedData.data() + 3, sizeof(id));
					j["routed"] = PacketDecoder::Name(static_cast<ServiceType>(service), id);
				}
			}) } },

			// Between servers: session keys are blanked when recorded
			{ K(S::MASTER, MessageType::Master::REQUEST_SESSION_KEY), { Make<MasterPackets::RequestSessionKey>([](const auto& p, json& j) { j = { {"username", p.username.GetAsString()} }; }),
				nullptr, Scrub<MasterPackets::RequestSessionKey>([](auto& p) { X(p.username); }) } },
			{ K(S::MASTER, MessageType::Master::SESSION_KEY_RESPONSE), {
				Make<MasterPackets::SessionKeyResponse>([](const auto& p, json& j) { j = { {"username", p.username.GetAsString()} }; }),
				Blank<MasterPackets::SessionKeyResponse>([](auto& p) { p.sessionKey = 0; }),
				Scrub<MasterPackets::SessionKeyResponse>([](auto& p) { X(p.username); }) } },
			{ K(S::MASTER, MessageType::Master::SET_SESSION_KEY), {
				Make<MasterPackets::SetSessionKey>([](const auto& p, json& j) { j = { {"username", p.username.string} }; }),
				Blank<MasterPackets::SetSessionKey>([](auto& p) { p.sessionKey = 0; }),
				Scrub<MasterPackets::SetSessionKey>([](auto& p) { X(p.username); }) } },
			{ K(S::MASTER, MessageType::Master::NEW_SESSION_ALERT), {
				Make<MasterPackets::NewSessionAlert>([](const auto& p, json& j) { j = { {"username", p.username.string} }; }),
				Blank<MasterPackets::NewSessionAlert>([](auto& p) { p.sessionKey = 0; }),
				Scrub<MasterPackets::NewSessionAlert>([](auto& p) { X(p.username); }) } },
			{ K(S::MASTER, MessageType::Master::REQUEST_ZONE_TRANSFER), { Make<MasterPackets::RequestZoneTransfer>([](const auto& p, json& j) {
				j = { {"requestID", Id(p.requestID)}, {"zoneID", p.zoneID}, {"cloneID", p.cloneID}, {"mythranShift", p.mythranShift}, {"stamps", p.stamps.size()} };
			}) } },
			{ K(S::MASTER, MessageType::Master::REQUEST_ZONE_TRANSFER_RESPONSE), { Make<MasterPackets::RequestZoneTransferResponse>([](const auto& p, json& j) {
				j = { {"requestID", Id(p.requestID)}, {"zoneID", p.zoneID}, {"zoneInstance", p.zoneInstance}, {"zoneClone", p.zoneClone}, {"serverPort", p.serverPort},
					{"stamps", p.stamps.size()} };
			}) } },
			{ K(S::MASTER, MessageType::Master::PLAYER_ADDED), { Make<MasterPackets::PlayerAdded>([](const auto& p, json& j) { j = { {"zoneID", p.zoneID}, {"instanceID", p.instanceID} }; }) } },
			{ K(S::MASTER, MessageType::Master::PLAYER_REMOVED), { Make<MasterPackets::PlayerRemoved>([](const auto& p, json& j) { j = { {"zoneID", p.zoneID}, {"instanceID", p.instanceID} }; }) } },
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
					out.fields = json{ {"networkID", network}, {"objectID", Id(object)}, {"lot", lot} };
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

		const auto it = Registry().find({ service, id });
		if (it == Registry().end()) return out;
		out.fields = it->second.fields(stream, fromClient);
		out.failed = !out.fields.has_value();
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

	size_t RegisteredCount() { return Registry().size() + 1; }
}
