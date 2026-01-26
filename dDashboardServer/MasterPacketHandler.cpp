#include "MasterPacketHandler.h"

#include "BitStreamUtils.h"
#include "dServer.h"
#include "Game.h"
#include "Logger.h"
#include "RakNetTypes.h"
#include "routes/ServerState.h"
#include <chrono>
#include <mutex>

namespace MasterPacketHandler {
	namespace {
		std::map<MessageType::Master, std::function<std::unique_ptr<MasterPacket>()>> g_Handlers = {
			{MessageType::Master::SERVER_INFO, []() {
				return std::make_unique<ServerInfo>();
			}},
			{MessageType::Master::PLAYER_ADDED, []() {
				return std::make_unique<PlayerAdded>();
			}},
			{MessageType::Master::PLAYER_REMOVED, []() {
				return std::make_unique<PlayerRemoved>();
			}},
			{MessageType::Master::SHUTDOWN_RESPONSE, []() {
				return std::make_unique<ShutdownResponse>();
			}},
			{MessageType::Master::SHUTDOWN, []() {
				return std::make_unique<Shutdown>();
			}},
		};
	}

	bool ServerInfo::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(port));
		VALIDATE_READ(bitStream.Read(zoneID));
		VALIDATE_READ(bitStream.Read(instanceID));
		VALIDATE_READ(bitStream.Read(serverType));
		LUString ipStr{};
		VALIDATE_READ(bitStream.Read(ipStr));
		ip = ipStr.string;
		return true;
	}

	void ServerInfo::Handle() {
		std::lock_guard<std::mutex> lock(ServerState::g_StatusMutex);

		LOG("MasterPacketHandler: Processing SERVER_INFO for service type %i, zone %u, instance %u, port %u", serverType, zoneID, instanceID, port);

		switch (serverType) {
			case ServiceType::AUTH:
				ServerState::g_AuthStatus.online = true;
				ServerState::g_AuthStatus.lastSeen = std::chrono::steady_clock::now();
				LOG("Updated Auth server status: online");
				break;
			case ServiceType::CHAT:
				ServerState::g_ChatStatus.online = true;
				ServerState::g_ChatStatus.lastSeen = std::chrono::steady_clock::now();
				LOG("Updated Chat server status: online");
				break;
			case ServiceType::WORLD: {
				// Update or add world instance
				bool found = false;
				for (auto& world : ServerState::g_WorldInstances) {
					if (world.mapID == zoneID && world.instanceID == instanceID) {
						world.ip = ip;
						world.port = port;
						found = true;
						break;
					}
				}
				if (!found) {
					WorldInstanceInfo info{};
					info.mapID = zoneID;
					info.instanceID = instanceID;
					info.cloneID = 0;
					info.players = 0;
					info.ip = ip;
					info.port = port;
					info.isPrivate = false;
					ServerState::g_WorldInstances.push_back(info);
					LOG("Added world instance: map %u instance %u", zoneID, instanceID);
				}
				break;
			}
			default:
				break;
		}
	}

	bool PlayerAdded::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(zoneID));
		VALIDATE_READ(bitStream.Read(instanceID));
		return true;
	}

	void PlayerAdded::Handle() {
		std::lock_guard<std::mutex> lock(ServerState::g_StatusMutex);
		for (auto& world : ServerState::g_WorldInstances) {
			if (world.mapID == zoneID && world.instanceID == instanceID) {
				world.players++;
				LOG_DEBUG("Player added to map %u instance %u, now %u players", zoneID, instanceID, world.players);
				break;
			}
		}
	}

	bool PlayerRemoved::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(zoneID));
		VALIDATE_READ(bitStream.Read(instanceID));
		return true;
	}

	void PlayerRemoved::Handle() {
		std::lock_guard<std::mutex> lock(ServerState::g_StatusMutex);
		for (auto& world : ServerState::g_WorldInstances) {
			if (world.mapID == zoneID && world.instanceID == instanceID) {
				if (world.players > 0) world.players--;
				LOG_DEBUG("Player removed from map %u instance %u, now %u players", zoneID, instanceID, world.players);
				break;
			}
		}
	}

	bool ShutdownResponse::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(zoneID));
		VALIDATE_READ(bitStream.Read(instanceID));
		VALIDATE_READ(bitStream.Read(serverType));
		return true;
	}

	void ShutdownResponse::Handle() {
		std::lock_guard<std::mutex> lock(ServerState::g_StatusMutex);

		switch (serverType) {
			case ServiceType::AUTH:
				ServerState::g_AuthStatus.online = false;
				LOG_DEBUG("Auth server shutdown");
				break;
			case ServiceType::CHAT:
				ServerState::g_ChatStatus.online = false;
				LOG_DEBUG("Chat server shutdown");
				break;
			case ServiceType::WORLD:
				for (auto it = ServerState::g_WorldInstances.begin(); it != ServerState::g_WorldInstances.end(); ++it) {
					if (it->mapID == zoneID && it->instanceID == instanceID) {
						ServerState::g_WorldInstances.erase(it);
						LOG_DEBUG("Removed shutdown instance: map %u instance %u", zoneID, instanceID);
						break;
					}
				}
				break;
			default:
				break;
		}
	}

	bool Shutdown::Deserialize(RakNet::BitStream& bitStream) {
		// SHUTDOWN message has no additional data
		return true;
	}

	void Shutdown::Handle() {
		LOG("Received SHUTDOWN command from Master");
		Game::lastSignal = -1; // Trigger shutdown
	}

	void HandleMasterPacket(Packet* packet) {
		if (!packet) return;

		switch (packet->data[0]) {
			case ID_DISCONNECTION_NOTIFICATION:
			case ID_CONNECTION_LOST:
				LOG("Lost connection to Master Server");
				{
					std::lock_guard<std::mutex> lock(ServerState::g_StatusMutex);
					ServerState::g_AuthStatus.online = false;
					ServerState::g_ChatStatus.online = false;
					ServerState::g_WorldInstances.clear();
				}
				break;
			case ID_CONNECTION_REQUEST_ACCEPTED:
				LOG("Connected to Master Server");
				break;
			case ID_USER_PACKET_ENUM: {
				RakNet::BitStream inStream(packet->data, packet->length, false);
				uint64_t header{};
				inStream.Read(header);

				const auto packetType = static_cast<MessageType::Master>(header);
				LOG_DEBUG("Received Master packet type: %i", packetType);

				auto it = g_Handlers.find(packetType);
				if (it != g_Handlers.end()) {
					auto handler = it->second();
					if (!handler->Deserialize(inStream)) {
						LOG_DEBUG("Error deserializing Master packet type %i", packetType);
						return;
					}
					handler->Handle();
				} else {
					LOG_DEBUG("Unhandled Master packet type: %i", packetType);
				}
				break;
			}
			default:
				break;
		}
	}
}
