#ifndef ACTIVITYMESSAGES_H
#define ACTIVITYMESSAGES_H

#include "GameMessages.h"
#include "NiQuaternion.h"
#include "eMatchUpdate.h"

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

	// Server -> client, to one client.
	struct MatchResponse : public NetGameMsg {
		MatchResponse() : NetGameMsg(MessageType::Game::MATCH_RESPONSE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		int32_t response{};
	};

	// Server -> client, to one client.
	struct MatchUpdate : public NetGameMsg {
		MatchUpdate() : NetGameMsg(MessageType::Game::MATCH_UPDATE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		// Name-value (LDF) text. Each byte is widened to one UTF-16 unit on the wire.
		std::string data{};
		eMatchUpdate type{};
	};

	// Client -> server. Join (type 0, value = activity ID) or ready/unready (type 1, value = ready) a match.
	struct MatchRequest : public NetGameMsg {
		MatchRequest() : NetGameMsg(MessageType::Game::MATCH_REQUEST) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		LWOOBJID activator{};
		// Name-value (LDF) text, narrowed to one byte per character as DLU always did.
		std::string playerChoices{};
		int32_t type{};
		int32_t value{};
	};

	// Client -> server (and server -> client in the client's definition). Answered with the leaderboard.
	struct RequestActivitySummaryLeaderboardData : public NetGameMsg {
		RequestActivitySummaryLeaderboardData() : NetGameMsg(MessageType::Game::REQUEST_ACTIVITY_SUMMARY_LEADERBOARD_DATA) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		int32_t gameID{ 0 }; // optional
		int32_t queryType{ 1 }; // optional, a Leaderboard::InfoType
		int32_t resultsEnd{ 10 }; // optional
		int32_t resultsStart{ 0 }; // optional
		LWOOBJID targetID{}; // "target" in the client; renamed so it doesn't hide NetGameMsg::target
		bool weekly{};
	};

	// Server -> client, to one client (UNASSIGNED broadcasts). The payload is written by Leaderboard::Serialize.
	// When a client sends it, nothing is read (DLU only logs it).
	struct SendActivitySummaryLeaderboardData : public NetGameMsg {
		SendActivitySummaryLeaderboardData() : NetGameMsg(MessageType::Game::SEND_ACTIVITY_SUMMARY_LEADERBOARD_DATA) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		const Leaderboard* leaderboard{};
	};

	// Server -> client, to one client (UNASSIGNED broadcasts).
	struct NotifyClientShootingGalleryScore : public NetGameMsg {
		NotifyClientShootingGalleryScore() : NetGameMsg(MessageType::Game::NOTIFY_CLIENT_SHOOTING_GALLERY_SCORE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		float addTime{};
		int32_t score{};
		LWOOBJID targetID{};
		NiPoint3 targetPos{};
	};

	// Client -> server. Nothing is done with it.
	struct UpdateShootingGalleryRotation : public NetGameMsg {
		UpdateShootingGalleryRotation() : NetGameMsg(MessageType::Game::UPDATE_SHOOTING_GALLERY_ROTATION) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		float angle{};
		NiPoint3 facing{};
		NiPoint3 muzzlePos{};
	};

	// Client -> server.
	struct ShootingGalleryFire : public NetGameMsg {
		ShootingGalleryFire() : NetGameMsg(MessageType::Game::SHOOTING_GALLERY_FIRE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		NiPoint3 target{};
		NiQuaternion rotation = QuatUtils::IDENTITY; // written w, x, y, z
	};

	// Client -> server.
	struct ActivityStateChangeRequest : public NetGameMsg {
		ActivityStateChangeRequest() : NetGameMsg(MessageType::Game::ACTIVITY_STATE_CHANGE_REQUEST) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		LWOOBJID objectID{};
		int32_t value1{};
		int32_t value2{};
		std::u16string stringValue{};
	};
};

#endif // ACTIVITYMESSAGES_H
