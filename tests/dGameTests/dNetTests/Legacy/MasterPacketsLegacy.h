#ifndef MASTERPACKETSLEGACY_H
#define MASTERPACKETSLEGACY_H

// FROZEN ORACLE - DO NOT EDIT.
// Verbatim copies of the hand written master packet code that dNet/MasterPackets.h replaced (dNet/MasterPackets.cpp,
// dNet/ZoneInstanceManager.cpp, dMasterServer/{MasterServer,InstanceManager}.cpp, dWorldServer/WorldServer.cpp,
// dDashboardServer/DashboardServer.cpp and the other senders at 9a392167, with the login stamps). Only the namespace changed, except:
//  - senders write into `bitStream` instead of sending it (SendToMaster and the master's peers aren't mocked);
//  - values the old code took from a server or an instance are passed in;
//  - old readers return what they read in a struct instead of acting on it.
// These messages never reach a client, but master, worlds, auth, chat and the dashboard can run different builds
// during an update, so their bytes are pinned too.

#include "LegacyPacketMacros.h"
#include "BitStreamUtils.h"
#include "dCommonVars.h"
#include "MessageType/Master.h"
#include "ServiceType.h"
#include "Stamps.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace LegacyMaster {
	// dNet/MasterPackets.cpp
	inline void SendZoneTransferRequest(RakNet::BitStream& bitStream, uint64_t requestID, bool mythranShift, uint32_t zoneID, uint32_t cloneID, const Stamps& stamps = {}) {
		BitStreamUtils::WriteHeader(bitStream, ServiceType::MASTER, MessageType::Master::REQUEST_ZONE_TRANSFER);

		bitStream.Write(requestID);
		bitStream.Write<uint8_t>(mythranShift);
		bitStream.Write(zoneID);
		bitStream.Write(cloneID);
		stamps.Serialize(bitStream);
	}

	inline void SendZoneCreatePrivate(RakNet::BitStream& bitStream, uint32_t zoneID, uint32_t cloneID, const std::string& password) {
		BitStreamUtils::WriteHeader(bitStream, ServiceType::MASTER, MessageType::Master::CREATE_PRIVATE_ZONE);

		bitStream.Write(zoneID);
		bitStream.Write(cloneID);

		bitStream.Write<uint32_t>(password.size());
		for (auto character : password) {
			bitStream.Write<char>(character);
		}
	}

	inline void SendZoneRequestPrivate(RakNet::BitStream& bitStream, uint64_t requestID, bool mythranShift, const std::string& password) {
		BitStreamUtils::WriteHeader(bitStream, ServiceType::MASTER, MessageType::Master::REQUEST_PRIVATE_ZONE);

		bitStream.Write(requestID);
		bitStream.Write<uint8_t>(mythranShift);

		bitStream.Write<uint32_t>(password.size());
		for (auto character : password) {
			bitStream.Write<char>(character);
		}
	}

	inline void SendWorldReady(RakNet::BitStream& bitStream, LWOMAPID zoneId, LWOINSTANCEID instanceId) {
		BitStreamUtils::WriteHeader(bitStream, ServiceType::MASTER, MessageType::Master::WORLD_READY);

		bitStream.Write(zoneId);
		bitStream.Write(instanceId);
	}

	inline void SendZoneTransferResponse(RakNet::BitStream& bitStream, uint64_t requestID, bool mythranShift, uint32_t zoneID, uint32_t zoneInstance, uint32_t zoneClone, const std::string& serverIP, uint32_t serverPort, const Stamps& stamps = {}) {
		BitStreamUtils::WriteHeader(bitStream, ServiceType::MASTER, MessageType::Master::REQUEST_ZONE_TRANSFER_RESPONSE);

		bitStream.Write(requestID);
		bitStream.Write<uint8_t>(mythranShift);
		bitStream.Write(zoneID);
		bitStream.Write(zoneInstance);
		bitStream.Write(zoneClone);
		bitStream.Write<uint16_t>(serverPort);
		bitStream.Write(LUString(serverIP, 255));
		stamps.Serialize(bitStream);
	}

	// SendServerInfo, with dServer's getters (int port, unsigned int zone, int instance) passed in
	inline void SendServerInfo(RakNet::BitStream& bitStream, int port, unsigned int zoneID, int instanceID, ServiceType serverType, const std::string& ip) {
		BitStreamUtils::WriteHeader(bitStream, ServiceType::MASTER, MessageType::Master::SERVER_INFO);

		bitStream.Write(port);
		bitStream.Write(zoneID);
		bitStream.Write(instanceID);
		bitStream.Write(serverType);
		bitStream.Write(LUString(ip));
	}

	// dMasterServer/MasterServer.cpp: a server went offline (to the dashboard)
	inline void WriteServerOffline(RakNet::BitStream& bitStream, ServiceType type) {
		BitStreamUtils::WriteHeader(bitStream, ServiceType::MASTER, MessageType::Master::SERVER_INFO);
		bitStream.Write<uint32_t>(0);
		bitStream.Write<uint32_t>(0);
		bitStream.Write<uint32_t>(0);
		bitStream.Write(type);
		bitStream.Write(LUString("offline"));
	}

	// dMasterServer/MasterServer.cpp: a world disconnected (to the dashboard)
	inline void WriteWorldShutDown(RakNet::BitStream& bitStream, LWOMAPID mapID, LWOINSTANCEID instanceID) {
		BitStreamUtils::WriteHeader(bitStream, ServiceType::MASTER, MessageType::Master::SHUTDOWN_RESPONSE);
		bitStream.Write(mapID);
		bitStream.Write(instanceID);
	}

	// dMasterServer/MasterServer.cpp: WORLD_READY (to the dashboard)
	inline void WriteWorldReadyInfo(RakNet::BitStream& bitStream, LWOMAPID zoneID, LWOINSTANCEID instanceID, LWOCLONEID cloneID, const std::string& ip, uint32_t port, bool isPrivate) {
		BitStreamUtils::WriteHeader(bitStream, ServiceType::MASTER, MessageType::Master::WORLD_READY);
		bitStream.Write(zoneID);
		bitStream.Write(instanceID);
		bitStream.Write(cloneID);
		bitStream.Write(LUString(ip));
		bitStream.Write(port);
		bitStream.Write<uint8_t>(isPrivate ? 1 : 0);
	}

	// dMasterServer/MasterServer.cpp SET_SESSION_KEY, and dNet/AuthPackets.cpp
	inline void WriteSessionKey(RakNet::BitStream& bitStream, MessageType::Master type, uint32_t sessionKey, const LUString& username) {
		BitStreamUtils::WriteHeader(bitStream, ServiceType::MASTER, type);
		bitStream.Write(sessionKey);
		bitStream.Write(username);
	}

	// dMasterServer/MasterServer.cpp REQUEST_SESSION_KEY
	inline void WriteSessionKeyResponse(RakNet::BitStream& bitStream, uint32_t sessionKey, const LUWString& username) {
		BitStreamUtils::WriteHeader(bitStream, ServiceType::MASTER, MessageType::Master::SESSION_KEY_RESPONSE);
		bitStream.Write(sessionKey);
		bitStream.Write(username);
	}

	// dWorldServer/WorldServer.cpp
	inline void WriteRequestSessionKey(RakNet::BitStream& bitStream, const LUWString& username) {
		BitStreamUtils::WriteHeader(bitStream, ServiceType::MASTER, MessageType::Master::REQUEST_SESSION_KEY);
		bitStream.Write(username);
	}

	// dWorldServer/WorldServer.cpp and dMasterServer/MasterServer.cpp (PLAYER_ADDED and PLAYER_REMOVED)
	inline void WritePlayerCount(RakNet::BitStream& bitStream, MessageType::Master type, LWOMAPID zoneID, LWOINSTANCEID instanceID) {
		BitStreamUtils::WriteHeader(bitStream, ServiceType::MASTER, type);
		bitStream.Write<LWOMAPID>(zoneID);
		bitStream.Write<LWOINSTANCEID>(instanceID);
	}

	// dMasterServer/InstanceManager.cpp and dWorldServer/WorldServer.cpp (AFFIRM_TRANSFER_REQUEST and _RESPONSE)
	inline void WriteAffirmTransfer(RakNet::BitStream& bitStream, MessageType::Master type, uint64_t requestID) {
		BitStreamUtils::WriteHeader(bitStream, ServiceType::MASTER, type);

		bitStream.Write(requestID);
	}

	// Messages with nothing after the header: SHUTDOWN, SHUTDOWN_RESPONSE (world), SHUTDOWN_UNIVERSE,
	// REQUEST_SERVER_LIST, CONFIG_RELOAD, DASHBOARD_SHUTDOWN
	inline void WriteEmpty(RakNet::BitStream& bitStream, MessageType::Master type) {
		BitStreamUtils::WriteHeader(bitStream, ServiceType::MASTER, type);
	}

	// dGame/dComponents/RocketLaunchpadControlComponent.cpp
	inline void TellMasterToPrepZone(RakNet::BitStream& bitStream, int zoneID) {
		BitStreamUtils::WriteHeader(bitStream, ServiceType::MASTER, MessageType::Master::PREP_ZONE);
		bitStream.Write(zoneID);
	}

	// dDashboardServer/routes/LiveWorld.cpp
	inline void WriteInstanceShutdown(RakNet::BitStream& bitStream, uint32_t zone, uint32_t instance) {
		BitStreamUtils::WriteHeader(bitStream, ServiceType::MASTER, MessageType::Master::INSTANCE_SHUTDOWN);
		bitStream.Write(zone);
		bitStream.Write(instance);
	}

	struct ServerListInstance {
		LWOMAPID mapID{};
		LWOINSTANCEID instanceID{};
		LWOCLONEID cloneID{};
		uint32_t players{};
		std::string ip;
		uint32_t port{};
		bool isPrivate{};
	};

	// dMasterServer/MasterServer.cpp REQUEST_SERVER_LIST, with the ready instances passed in
	inline void WriteServerList(RakNet::BitStream& bitStream, bool authOnline, bool chatOnline, const std::vector<ServerListInstance>& instances) {
		BitStreamUtils::WriteHeader(bitStream, ServiceType::MASTER, MessageType::Master::SERVER_LIST_RESPONSE);

		bitStream.Write<uint8_t>(authOnline ? 1 : 0);
		bitStream.Write<uint8_t>(chatOnline ? 1 : 0);

		uint32_t instanceCount = instances.size();
		bitStream.Write(instanceCount);

		for (const auto& inst : instances) {
			bitStream.Write(inst.mapID);
			bitStream.Write(inst.instanceID);
			bitStream.Write(inst.cloneID);
			bitStream.Write<uint32_t>(inst.players);
			bitStream.Write(LUString(inst.ip));
			bitStream.Write(inst.port);
			bitStream.Write<uint8_t>(inst.isPrivate ? 1 : 0);
		}
	}

	// Readers. The stream is past the packet header.

	struct TransferRequestRead {
		uint64_t requestID = 0;
		uint8_t mythranShift = false;
		uint32_t zoneID = 0;
		uint32_t zoneClone = 0;
		Stamps stamps;
	};

	// dMasterServer/MasterServer.cpp REQUEST_ZONE_TRANSFER
	inline TransferRequestRead ReadZoneTransferRequest(RakNet::BitStream& inStream) {
		uint64_t requestID = 0;
		uint8_t mythranShift = false;
		uint32_t zoneID = 0;
		uint32_t zoneClone = 0;

		inStream.Read(requestID);
		inStream.Read(mythranShift);
		inStream.Read(zoneID);
		inStream.Read(zoneClone);
		// The login stamps travelling with the request (see Stamps.h)
		Stamps stamps;
		if (!stamps.Deserialize(inStream)) stamps = {};
		return { requestID, mythranShift, zoneID, zoneClone, stamps };
	}

	struct TransferResponseRead {
		uint64_t requestID{};
		bool mythranShift{};
		uint32_t zoneID{};
		uint32_t zoneInstance{};
		uint32_t zoneClone{};
		uint16_t serverPort{};
		std::string serverIP;
		Stamps stamps;
	};

	// dNet/ZoneInstanceManager.cpp HandleRequestZoneTransferResponse
	inline TransferResponseRead ReadZoneTransferResponse(RakNet::BitStream& inStream) {
		uint64_t requestID;
		inStream.Read(requestID);
		bool mythranShift;
		uint8_t tmp;
		inStream.Read(tmp);
		mythranShift = tmp > 0;
		uint32_t zoneID;
		inStream.Read(zoneID);
		uint32_t zoneInstance;
		inStream.Read(zoneInstance);
		uint32_t zoneClone;
		inStream.Read(zoneClone);
		uint16_t serverPort;
		inStream.Read(serverPort);
		LUString serverIP(255);
		inStream.Read(serverIP);
		Stamps stamps;
		if (!stamps.Deserialize(inStream)) stamps = {};
		return { requestID, mythranShift, zoneID, zoneInstance, zoneClone, serverPort, serverIP.string, stamps };
	}

	struct ServerInfoRead {
		uint32_t theirPort = 0;
		uint32_t theirZoneID = 0;
		uint32_t theirInstanceID = 0;
		ServiceType theirServerType;
		std::string theirIP;
	};

	// dMasterServer/MasterServer.cpp and dDashboardServer/DashboardServer.cpp SERVER_INFO
	inline ServerInfoRead ReadServerInfo(RakNet::BitStream& inStream) {
		uint32_t theirPort = 0;
		uint32_t theirZoneID = 0;
		uint32_t theirInstanceID = 0;
		ServiceType theirServerType;
		LUString theirIP;

		inStream.Read(theirPort);
		inStream.Read(theirZoneID);
		inStream.Read(theirInstanceID);
		inStream.Read(theirServerType);
		inStream.Read(theirIP);
		return { theirPort, theirZoneID, theirInstanceID, theirServerType, theirIP.string };
	}

	// dMasterServer/MasterServer.cpp CREATE_PRIVATE_ZONE
	inline std::pair<std::pair<uint32_t, LWOCLONEID>, std::string> ReadCreatePrivateZone(RakNet::BitStream& inStream) {
		uint32_t mapId;
		LWOCLONEID cloneId;
		std::string password;

		inStream.Read(mapId);
		inStream.Read(cloneId);

		uint32_t len;
		inStream.Read<uint32_t>(len);
		len = std::min<uint32_t>(len, 50); // cap the master password at 50 characters

		for (uint32_t i = 0; len > i; i++) {
			char character;
			inStream.Read<char>(character);
			password += character;
		}
		return { { mapId, cloneId }, password };
	}

	// dMasterServer/MasterServer.cpp REQUEST_PRIVATE_ZONE
	inline std::pair<std::pair<uint64_t, uint8_t>, std::string> ReadRequestPrivateZone(RakNet::BitStream& inStream) {
		uint64_t requestID = 0;
		uint8_t mythranShift = false;

		std::string password;

		inStream.Read(requestID);
		inStream.Read(mythranShift);

		uint32_t len;
		inStream.Read<uint32_t>(len);
		len = std::min<uint32_t>(len, 50);

		for (uint32_t i = 0; i < len; i++) {
			char character; inStream.Read<char>(character);
			password += character;
		}
		return { { requestID, mythranShift }, password };
	}

	struct ServerListRead {
		uint8_t authOnline = 0;
		uint8_t chatOnline = 0;
		std::vector<ServerListInstance> instances;
	};

	// dDashboardServer/DashboardServer.cpp SERVER_LIST_RESPONSE
	inline ServerListRead ReadServerList(RakNet::BitStream& inStream) {
		ServerListRead out;
		uint8_t authOnline = 0;
		uint8_t chatOnline = 0;
		uint32_t instanceCount = 0;

		inStream.Read(authOnline);
		inStream.Read(chatOnline);
		inStream.Read(instanceCount);
		out.authOnline = authOnline;
		out.chatOnline = chatOnline;

		for (uint32_t i = 0; i < instanceCount; i++) {
			ServerListInstance info;
			LUString ip;
			// Same types as MasterServer writes them (map and instance IDs are 16 bits)
			LWOMAPID mapID = 0;
			LWOINSTANCEID instanceID = 0;
			inStream.Read(mapID);
			inStream.Read(instanceID);
			info.mapID = mapID;
			info.instanceID = instanceID;
			inStream.Read(info.cloneID);
			inStream.Read(info.players);
			inStream.Read(ip);
			info.ip = ip.string;
			inStream.Read(info.port);
			uint8_t isPrivate = 0;
			inStream.Read(isPrivate);
			info.isPrivate = isPrivate != 0;
			out.instances.push_back(info);
		}
		return out;
	}

	// dDashboardServer/DashboardServer.cpp WORLD_READY
	inline ServerListInstance ReadWorldReadyInfo(RakNet::BitStream& inStream) {
		LWOMAPID zoneID;
		LWOINSTANCEID instanceID;
		LWOCLONEID cloneID;
		LUString ip;
		uint32_t port;
		uint8_t isPrivate;

		inStream.Read(zoneID);
		inStream.Read(instanceID);
		inStream.Read(cloneID);
		inStream.Read(ip);
		inStream.Read(port);
		inStream.Read(isPrivate);
		return { zoneID, instanceID, cloneID, 0, ip.string, port, isPrivate != 0 };
	}
}

#endif // MASTERPACKETSLEGACY_H
