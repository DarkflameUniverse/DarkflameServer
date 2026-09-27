#ifndef QUICKBUILDMESSAGES_H
#define QUICKBUILDMESSAGES_H

#include "GameMessages.h"
#include "eQuickBuildFailReason.h"
#include "eQuickBuildState.h"

// Game messages for quickbuilds (rebuilds in the client).
// Fields are listed in wire order; names follow the client (legouniverse.exe 1.10.64) where known.
namespace GameMessages {
	// Server -> client, broadcast.
	struct RebuildNotifyState : public NetGameMsg {
		RebuildNotifyState() : NetGameMsg(MessageType::Game::REBUILD_NOTIFY_STATE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		eQuickBuildState prevState{};
		eQuickBuildState state{};
		LWOOBJID player{};
	};

	// Server -> client, broadcast.
	struct EnableRebuild : public NetGameMsg {
		EnableRebuild() : NetGameMsg(MessageType::Game::ENABLE_REBUILD) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		bool bEnable{};
		bool bFail{};
		bool bSuccess{};
		eQuickBuildFailReason eFailReason{ eQuickBuildFailReason::NOT_GIVEN }; // optional
		float fDuration{};
		LWOOBJID user{};
	};

	// Client -> server.
	struct RebuildCancel : public NetGameMsg {
		RebuildCancel() : NetGameMsg(MessageType::Game::REBUILD_CANCEL) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		bool bEarlyRelease{};
		LWOOBJID userID{};
	};
}

#endif // QUICKBUILDMESSAGES_H
