#include "CommonPackets.h"

#include "dConfig.h"
#include "dServer.h"
#include "eServerDisconnectIdentifiers.h"
#include "Game.h"
#include "GeneralUtils.h"
#include "Logger.h"
#include "ServiceType.h"
#include "StringifiedEnum.h"

#include <functional>
#include <map>
#include <memory>

namespace CommonPackets {
	void ClientVersionConfirm::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(netVersion);
		bitStream.Write(unknown);
		bitStream.Write(serviceType);
		bitStream.Write(padding);
		bitStream.Write(processID);
		bitStream.Write(port);
		bitStream.Write(unknown2);
	}

	bool ClientVersionConfirm::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(netVersion));
		VALIDATE_READ(bitStream.Read(unknown));
		VALIDATE_READ(bitStream.Read(serviceType));
		VALIDATE_READ(bitStream.Read(padding));
		VALIDATE_READ(bitStream.Read(processID));
		VALIDATE_READ(bitStream.Read(port));
		VALIDATE_READ(bitStream.Read(unknown2));
		return true;
	}

	void ClientVersionConfirm::Handle() {
		if (serviceType != ServiceType::CLIENT) LOG("WARNING: Service is not a Client!");
		if (port != sysAddr.port) LOG("WARNING: Port written in packet does not match the port the client is connecting over!");

		LOG_DEBUG("Client Data [Version: %i, Service: %s, Process: %u, Port: %u, Sysaddr Port: %u]", netVersion, StringifiedEnum::ToString(serviceType).data(), processID, port, sysAddr.port);

		ServerVersionConfirm response;
		const auto& clientNetVersionString = Game::config->GetValue("client_net_version");
		response.netVersion = GeneralUtils::TryParse<uint32_t>(clientNetVersionString).value_or(ServerVersionConfirm::DEFAULT_NET_VERSION);
		response.serviceType = static_cast<uint32_t>(Game::server->GetServerType());
		response.Send(sysAddr);
	}

	void ServerVersionConfirm::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(netVersion);
		bitStream.Write(unknown);
		bitStream.Write(serviceType);
		bitStream.Write(versionMajor);
		bitStream.Write(versionMinor);
		bitStream.Write(versionPatch);
		bitStream.Write(buildFlags);
		for (int shift = 24; shift >= 0; shift -= 8) bitStream.Write<uint8_t>(commitPrefix >> shift);
		BitStreamUtils::WriteLengthPrefixed<uint16_t>(bitStream, buildString);
	}

	bool ServerVersionConfirm::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(netVersion));
		VALIDATE_READ(bitStream.Read(unknown));
		VALIDATE_READ(bitStream.Read(serviceType));
		VALIDATE_READ(bitStream.Read(versionMajor));
		VALIDATE_READ(bitStream.Read(versionMinor));
		VALIDATE_READ(bitStream.Read(versionPatch));
		VALIDATE_READ(bitStream.Read(buildFlags));
		commitPrefix = 0;
		for (int i = 0; i < 4; i++) {
			uint8_t byte{};
			VALIDATE_READ(bitStream.Read(byte));
			commitPrefix = (commitPrefix << 8) | byte;
		}
		// Older DLU servers stop after the 12 fixed bytes.
		buildString.clear();
		if (bitStream.GetNumberOfUnreadBits() == 0) return true;
		return BitStreamUtils::ReadLengthPrefixed<uint16_t>(bitStream, buildString, MAX_BUILD_STRING_LENGTH);
	}

	void DisconnectNotify::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(disconnectID);
	}

	bool DisconnectNotify::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(disconnectID));
		return true;
	}

	void GeneralNotify::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(notifyType);
		bitStream.Write<uint8_t>(showMessageBox);
	}

	bool GeneralNotify::Deserialize(RakNet::BitStream& bitStream) {
		uint8_t show{};
		VALIDATE_READ(bitStream.Read(notifyType));
		VALIDATE_READ(bitStream.Read(show));
		showMessageBox = show != 0;
		return true;
	}

	namespace {
		// Only what a client sends; the other COMMON packets go from server to client.
		const std::map<MessageType::Server, std::function<std::unique_ptr<ClientVersionConfirm>()>> g_Handlers = {
			{ MessageType::Server::VERSION_CONFIRM, []() { return std::make_unique<ClientVersionConfirm>(); } },
		};
	}

	void Handle(RakNet::BitStream& inStream, const SystemAddress& sysAddr, const uint32_t packetID) {
		const auto messageID = static_cast<MessageType::Server>(packetID);
		const auto it = g_Handlers.find(messageID);
		if (it == g_Handlers.end()) {
			LOG_DEBUG("Unhandled common packet %i", packetID);
			return;
		}

		auto request = it->second();
		request->sysAddr = sysAddr;
		if (!request->Deserialize(inStream)) {
			LOG("Failed to read common packet %s", StringifiedEnum::ToString(messageID).data());
			return;
		}
		request->Handle();
	}
}
