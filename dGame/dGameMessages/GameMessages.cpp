#include "GameMessages.h"

#include "BitStream.h"
#include "BitStreamUtils.h"
#include "EntityManager.h"
#include "Game.h"
#include "MessageType/Client.h"
#include "ServiceType.h"
#include "dServer.h"

namespace GameMessages {
	bool GameMsg::Send() {
		return Game::entityManager->SendMessage(*this);
	}

	bool GameMsg::Send(const LWOOBJID _target) {
		target = _target;
		return Send();
	}

	void NetGameMsg::WritePacket(RakNet::BitStream& bitStream) const {
		LUBitStream(ServiceType::CLIENT, MessageType::Client::GAME_MSG).WriteHeader(bitStream);

		bitStream.Write(target); // Who this message will be sent to on the (a) client
		bitStream.Write(msgId); // the ID of this message

		Serialize(bitStream); // write the message data
	}

	bool NetGameMsg::ReadPacketHeader(RakNet::BitStream& bitStream, LWOOBJID& target, MessageType::Game& msgId) {
		LUBitStream header;
		if (!header.ReadHeader(bitStream)) return false;
		if (header.connectionType != ServiceType::CLIENT || header.internalPacketID != static_cast<uint32_t>(MessageType::Client::GAME_MSG)) return false;
		VALIDATE_READ(bitStream.Read(target));
		VALIDATE_READ(bitStream.Read(msgId));
		return true;
	}

	void NetGameMsg::SendToClient(const SystemAddress& sysAddr) const {
		RakNet::BitStream bitStream;
		WritePacket(bitStream);
		Game::server->Send(bitStream, sysAddr, false);
	}

	void NetGameMsg::BroadcastExcept(const SystemAddress& excluded) const {
		RakNet::BitStream bitStream;
		WritePacket(bitStream);
		Game::server->Send(bitStream, excluded, true);
	}

	void NetGameMsg::Send(const SystemAddress& sysAddr) const {
		RakNet::BitStream bitStream;
		WritePacket(bitStream);

		// Send to everyone if someone sent unassigned system address, or to one specific client.
		Game::server->Send(bitStream, sysAddr, sysAddr == UNASSIGNED_SYSTEM_ADDRESS);
	}
}
