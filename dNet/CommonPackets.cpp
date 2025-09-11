
#include "CommonPackets.h"
#include "dServer.h"
#include "Logger.h"
#include "Game.h"
#include "dServer.h"
#include "dConfig.h"
#include "StringifiedEnum.h"
#include "GeneralUtils.h"


namespace CommonPackets {
	std::map<MessageType::Server, std::function<std::unique_ptr<LUBitStream>()>> g_Handlers = {
		{MessageType::Server::VERSION_CONFIRM, []() {
			return std::make_unique<VersionConfirm>();
		}},
		{MessageType::Server::DISCONNECT_NOTIFY, []() {
			return std::make_unique<DisconnectNotify>();
		}},
		{MessageType::Server::GENERAL_NOTIFY, []() {
			return std::make_unique<GeneralNotify>();
		}}
	};

	void VersionConfirm::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write<uint32_t>(netVersion);
		bitStream.Write<uint32_t>(861228100);
		bitStream.Write(static_cast<uint32_t>(serviceType));
		bitStream.Write<uint64_t>(219818307120);
	}

	bool VersionConfirm::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(netVersion));
		uint32_t unknown = 0;
		VALIDATE_READ(bitStream.Read(unknown));
		VALIDATE_READ(bitStream.Read(serviceType));
		uint16_t unknown2 = 0;
		VALIDATE_READ(bitStream.Read(unknown2));
		VALIDATE_READ(bitStream.Read(processID));
		VALIDATE_READ(bitStream.Read(port));
		LUString unknownString;
		VALIDATE_READ(bitStream.Read(unknownString));
		return true;
	}

	void VersionConfirm::Handle() {
		LOG_DEBUG("Client Data [Version: %i, Service: %s, Process: %u, Port: %u, Sysaddr Port: %u]", netVersion, StringifiedEnum::ToString(serviceType).data(), processID, port, sysAddr.port);

		VersionConfirm response;
		auto& serverNetVersionString = Game::config->GetValue("client_net_version");
		const uint32_t serverNetVersion = GeneralUtils::TryParse<uint32_t>(serverNetVersionString).value_or(171022);
		response.netVersion = serverNetVersion;
		response.serviceType = Game::server->GetServerType();
		response.Send(sysAddr);
	}

	void DisconnectNotify::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(disconnectID);
	}

	void GeneralNotify::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(notifyID);
		bitStream.Write(notifyUser);
	}
}

void CommonPackets::Handle(RakNet::BitStream& inStream, const SystemAddress& sysAddr) {
	inStream.ResetReadPointer();
	LUBitStream lubitstream;
	if (!lubitstream.ReadHeader(inStream)) return;

	auto it = g_Handlers.find(static_cast<MessageType::Server>(lubitstream.internalPacketID));
	if (it != g_Handlers.end()) {
		auto request = it->second();
		request->sysAddr = sysAddr;
		if (!request->Deserialize(inStream)) {
			LOG_DEBUG("Error Reading Common Packet: %s", StringifiedEnum::ToString(static_cast<MessageType::Server>(lubitstream.internalPacketID)).data());
			return;
		}
		LOG_DEBUG("Received Common Packet: %s", StringifiedEnum::ToString(static_cast<MessageType::Server>(lubitstream.internalPacketID)).data());
		request->Handle();
	} else {
		LOG_DEBUG("Unhandled Common Packet with ID: %i", lubitstream.internalPacketID);
	}
}

