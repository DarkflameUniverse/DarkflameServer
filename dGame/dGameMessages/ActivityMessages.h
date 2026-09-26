#ifndef ACTIVITYMESSAGES_H
#define ACTIVITYMESSAGES_H

#include "GameMessages.h"

#include <string>

// Game messages for the activity lifecycle (minigames, races, shooting galleries, survival...).
// Field names and wire order match the client (legouniverse.exe 1.10.64). Fields are listed in wire order.
namespace GameMessages {
	// Server -> client. No payload.
	struct ActivityEnter : public NetGameMsg {
		ActivityEnter() : NetGameMsg(MessageType::Game::ACTIVITY_ENTER) {}
	};

	// Server -> client. No payload.
	struct ActivityExit : public NetGameMsg {
		ActivityExit() : NetGameMsg(MessageType::Game::ACTIVITY_EXIT) {}
	};

	// Server -> client. No payload.
	struct ActivityStart : public NetGameMsg {
		ActivityStart() : NetGameMsg(MessageType::Game::ACTIVITY_START) {}
	};

	// Server -> client.
	struct ActivityStop : public NetGameMsg {
		ActivityStop() : NetGameMsg(MessageType::Game::ACTIVITY_STOP) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		bool bExit{};
		bool bUserCancel{};
	};

	// Server -> client.
	struct ActivityPause : public NetGameMsg {
		ActivityPause() : NetGameMsg(MessageType::Game::ACTIVITY_PAUSE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		bool bPause{};
	};

	// Server -> client.
	struct StartActivityTime : public NetGameMsg {
		StartActivityTime() : NetGameMsg(MessageType::Game::START_ACTIVITY_TIME) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		float startTime{};
	};

	// Client -> server in the client, but historically also sent by DLU. Kept for completeness.
	struct RequestActivityEnter : public NetGameMsg {
		RequestActivityEnter() : NetGameMsg(MessageType::Game::REQUEST_ACTIVITY_ENTER) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		bool bStart{};
		LWOOBJID userID{};
	};

	// Client -> server. Sent when the player leaves an activity (e.g. closes the minigame UI).
	struct RequestActivityExit : public NetGameMsg {
		RequestActivityExit() : NetGameMsg(MessageType::Game::REQUEST_ACTIVITY_EXIT) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		bool bUserCancel{};
		LWOOBJID userID{};
	};

	// Server -> client.
	struct ShowActivityCountdown : public NetGameMsg {
		ShowActivityCountdown() : NetGameMsg(MessageType::Game::SHOW_ACTIVITY_COUNTDOWN) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		bool bPlayAdditionalSound{};
		bool bPlayCountdownSound{};
		std::u16string sndName{};
		int32_t stateToPlaySoundOn{};
	};
};

#endif // ACTIVITYMESSAGES_H
