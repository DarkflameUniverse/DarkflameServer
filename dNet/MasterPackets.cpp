#include "MasterPackets.h"
#include "BitStream.h"
#include "dCommonVars.h"
#include "dServer.h"
#include "Game.h"
#include "ServiceType.h"
#include "MessageType/Master.h"
#include "BitStreamUtils.h"

#include <algorithm>
#include <string>

namespace {
	// u32 length and 1 byte per character. A longer string is cut to maxLength when read (the rest is skipped).
	bool ReadCappedString(RakNet::BitStream& bitStream, std::string& value, const uint32_t maxLength) {
		uint32_t length{};
		VALIDATE_READ(bitStream.Read(length));
		value.resize(std::min(length, maxLength));
		return value.empty() || bitStream.ReadBits(reinterpret_cast<unsigned char*>(value.data()), BYTES_TO_BITS(value.size()), true);
	}
}

void MasterPackets::SendToMaster(const LUBitStream& msg, dServer* server) {
	RakNet::BitStream bitStream;
	msg.WritePacket(bitStream);
	(server ? server : Game::server)->SendToMaster(bitStream);
}

void MasterPackets::SendTo(const SystemAddress& sysAddr, const LUBitStream& msg) {
	RakNet::BitStream bitStream;
	msg.WritePacket(bitStream);
	Game::server->Send(bitStream, sysAddr, false);
}

namespace MasterPackets {
	void RequestZoneTransfer::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(requestID);
		bitStream.Write(mythranShift);
		bitStream.Write(zoneID);
		bitStream.Write(cloneID);
		stamps.Serialize(bitStream);
	}

	bool RequestZoneTransfer::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(requestID));
		VALIDATE_READ(bitStream.Read(mythranShift));
		VALIDATE_READ(bitStream.Read(zoneID));
		VALIDATE_READ(bitStream.Read(cloneID));
		if (!stamps.Deserialize(bitStream)) stamps = {};
		return true;
	}

	void RequestZoneTransferResponse::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(requestID);
		bitStream.Write(mythranShift);
		bitStream.Write(zoneID);
		bitStream.Write(zoneInstance);
		bitStream.Write(zoneClone);
		bitStream.Write(serverPort);
		bitStream.Write(serverIP);
		stamps.Serialize(bitStream);
	}

	bool RequestZoneTransferResponse::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(requestID));
		VALIDATE_READ(bitStream.Read(mythranShift));
		VALIDATE_READ(bitStream.Read(zoneID));
		VALIDATE_READ(bitStream.Read(zoneInstance));
		VALIDATE_READ(bitStream.Read(zoneClone));
		VALIDATE_READ(bitStream.Read(serverPort));
		VALIDATE_READ(bitStream.Read(serverIP));
		if (!stamps.Deserialize(bitStream)) stamps = {};
		return true;
	}

	void ServerInfo::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(port);
		bitStream.Write(zoneID);
		bitStream.Write(instanceID);
		bitStream.Write(serverType);
		bitStream.Write(ip);
	}

	bool ServerInfo::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(port));
		VALIDATE_READ(bitStream.Read(zoneID));
		VALIDATE_READ(bitStream.Read(instanceID));
		VALIDATE_READ(bitStream.Read(serverType));
		VALIDATE_READ(bitStream.Read(ip));
		return true;
	}

	void RequestSessionKey::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(username);
	}

	bool RequestSessionKey::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(username));
		return true;
	}

	void SetSessionKey::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(sessionKey);
		bitStream.Write(username);
	}

	bool SetSessionKey::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(sessionKey));
		VALIDATE_READ(bitStream.Read(username));
		return true;
	}

	void SessionKeyResponse::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(sessionKey);
		bitStream.Write(username);
	}

	bool SessionKeyResponse::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(sessionKey));
		VALIDATE_READ(bitStream.Read(username));
		return true;
	}

	void NewSessionAlert::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(sessionKey);
		bitStream.Write(username);
	}

	bool NewSessionAlert::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(sessionKey));
		VALIDATE_READ(bitStream.Read(username));
		return true;
	}

	void PlayerCountChange::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(zoneID);
		bitStream.Write(instanceID);
	}

	bool PlayerCountChange::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(zoneID));
		VALIDATE_READ(bitStream.Read(instanceID));
		return true;
	}

	void CreatePrivateZone::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(zoneID);
		bitStream.Write(cloneID);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, password);
	}

	bool CreatePrivateZone::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(zoneID));
		VALIDATE_READ(bitStream.Read(cloneID));
		VALIDATE_READ(ReadCappedString(bitStream, password, MAX_PASSWORD_LENGTH));
		return true;
	}

	void RequestPrivateZone::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(requestID);
		bitStream.Write(mythranShift);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, password);
	}

	bool RequestPrivateZone::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(requestID));
		VALIDATE_READ(bitStream.Read(mythranShift));
		VALIDATE_READ(ReadCappedString(bitStream, password, MAX_PASSWORD_LENGTH));
		return true;
	}

	void WorldReady::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(zoneID);
		bitStream.Write(instanceID);
	}

	bool WorldReady::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(zoneID));
		VALIDATE_READ(bitStream.Read(instanceID));
		return true;
	}

	void WorldReadyInfo::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(zoneID);
		bitStream.Write(instanceID);
		bitStream.Write(cloneID);
		bitStream.Write(ip);
		bitStream.Write(port);
		bitStream.Write(isPrivate);
	}

	bool WorldReadyInfo::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(zoneID));
		VALIDATE_READ(bitStream.Read(instanceID));
		VALIDATE_READ(bitStream.Read(cloneID));
		VALIDATE_READ(bitStream.Read(ip));
		VALIDATE_READ(bitStream.Read(port));
		VALIDATE_READ(bitStream.Read(isPrivate));
		return true;
	}

	void PrepZone::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(zoneID);
	}

	bool PrepZone::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(zoneID));
		return true;
	}

	void WorldShutDown::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(zoneID);
		bitStream.Write(instanceID);
	}

	bool WorldShutDown::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(zoneID));
		VALIDATE_READ(bitStream.Read(instanceID));
		return true;
	}

	void AffirmTransfer::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(requestID);
	}

	bool AffirmTransfer::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(requestID));
		return true;
	}

	void ServerListResponse::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(authOnline);
		bitStream.Write(chatOnline);
		bitStream.Write<uint32_t>(instances.size());
		for (const auto& instance : instances) {
			bitStream.Write(instance.mapID);
			bitStream.Write(instance.instanceID);
			bitStream.Write(instance.cloneID);
			bitStream.Write(instance.players);
			bitStream.Write(instance.ip);
			bitStream.Write(instance.port);
			bitStream.Write(instance.isPrivate);
		}
		bitStream.Write(ugcEnabled);
		bitStream.Write(ugcOnline);
		bitStream.Write(ugcPid);
		for (const auto& instance : instances) bitStream.Write(static_cast<uint8_t>(instance.state));
	}

	bool ServerListResponse::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(authOnline));
		VALIDATE_READ(bitStream.Read(chatOnline));
		uint32_t count{};
		VALIDATE_READ(bitStream.Read(count));
		if (count > MAX_INSTANCES) return false;
		instances.resize(count);
		for (auto& instance : instances) {
			VALIDATE_READ(bitStream.Read(instance.mapID));
			VALIDATE_READ(bitStream.Read(instance.instanceID));
			VALIDATE_READ(bitStream.Read(instance.cloneID));
			VALIDATE_READ(bitStream.Read(instance.players));
			VALIDATE_READ(bitStream.Read(instance.ip));
			VALIDATE_READ(bitStream.Read(instance.port));
			VALIDATE_READ(bitStream.Read(instance.isPrivate));
		}
		VALIDATE_READ(bitStream.Read(ugcEnabled));
		VALIDATE_READ(bitStream.Read(ugcOnline));
		VALIDATE_READ(bitStream.Read(ugcPid));
		for (auto& instance : instances) {
			uint8_t state{};
			VALIDATE_READ(bitStream.Read(state));
			if (state > static_cast<uint8_t>(eState::DRAINING)) return false;
			instance.state = static_cast<eState>(state);
		}
		return true;
	}

	void InstanceShutdown::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(zoneID);
		bitStream.Write(instanceID);
	}

	bool InstanceShutdown::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(zoneID));
		VALIDATE_READ(bitStream.Read(instanceID));
		return true;
	}
}
