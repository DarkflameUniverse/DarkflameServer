#ifndef MISSIONMESSAGES_H
#define MISSIONMESSAGES_H

#include "GameMessages.h"

#include <vector>

enum class eMissionState : int;
enum class eMissionLockState : int;

// Game messages for missions, player flags, collectibles and level rewards.
// Field names follow the client (legouniverse.exe 1.10.64); fields are listed in wire order.
// The server -> client messages here are sent with SendToClient: they were never broadcast.
namespace GameMessages {
	// Server -> client. Sent twice by MissionOfferComponent: once targeting the offerer (zooms the camera to it)
	// and once targeting the player (opens the offer UI).
	struct OfferMission : public NetGameMsg {
		OfferMission() : NetGameMsg(MessageType::Game::OFFER_MISSION) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		int32_t missionID{};
		LWOOBJID offerer{};
	};

	// Client -> server.
	struct RespondToMission : public NetGameMsg {
		RespondToMission() : NetGameMsg(MessageType::Game::RESPOND_TO_MISSION) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		int32_t missionID{};
		LWOOBJID playerID{};
		LWOOBJID receiver{};
		LOT rewardItem{ LOT_NULL }; // optional
	};

	// Server -> client.
	struct NotifyMission : public NetGameMsg {
		NotifyMission() : NetGameMsg(MessageType::Game::NOTIFY_MISSION) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		int32_t missionID{};
		int32_t missionState{};
		bool sendingRewards{};
	};

	// Server -> client.
	struct NotifyMissionTask : public NetGameMsg {
		NotifyMissionTask() : NetGameMsg(MessageType::Game::NOTIFY_MISSION_TASK) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		int32_t missionID{};
		int32_t taskMask{};
		std::vector<float> updates{}; // u8 count, then the floats
	};

	// Server -> client.
	struct ResetMissions : public NetGameMsg {
		ResetMissions() : NetGameMsg(MessageType::Game::RESET_MISSIONS) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		int32_t missionID{ -1 }; // optional
	};

	// Client -> server.
	struct MissionDialogueOK : public NetGameMsg {
		MissionDialogueOK() : NetGameMsg(MessageType::Game::MISSION_DIALOGUE_OK) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		bool bIsComplete{};
		eMissionState iMissionState{};
		int32_t missionID{};
		LWOOBJID responder{};
	};

	// Client -> server. The player closed a mission offer without accepting. Nothing to do: the client carries on.
	struct MissionDialogueCancelled : public NetGameMsg {
		MissionDialogueCancelled() : NetGameMsg(MessageType::Game::MISSION_DIALOGUE_CANCELLED) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		bool bIsComplete{};
		eMissionState iMissionState{};
		int32_t missionID{};
		LWOOBJID responder{};
	};

	// Client -> server.
	struct RequestLinkedMission : public NetGameMsg {
		RequestLinkedMission() : NetGameMsg(MessageType::Game::REQUEST_LINKED_MISSION) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		LWOOBJID playerID{};
		int32_t missionID{};
		bool bMissionOffered{};
	};

	// Client -> server.
	struct SetFlag : public NetGameMsg {
		SetFlag() : NetGameMsg(MessageType::Game::SET_FLAG) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		bool bFlag{};
		int32_t iFlagID{};
	};

	// Server -> client.
	struct NotifyClientFlagChange : public NetGameMsg {
		NotifyClientFlagChange() : NetGameMsg(MessageType::Game::NOTIFY_CLIENT_FLAG_CHANGE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		bool bFlag{};
		uint32_t iFlagID{};
	};

	// Client -> server. Sent when the player collects a collectible.
	struct HasBeenCollected : public NetGameMsg {
		HasBeenCollected() : NetGameMsg(MessageType::Game::HAS_BEEN_COLLECTED) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		LWOOBJID playerID{};
	};

	// Client -> server. LWOMissionComponent sends it when a mission of a journal type/subtype (Missions.defined_type,
	// defined_subtype) is offered or updated, to mark that journal tab. Every live sample carried the default state
	// (NEW). The server keeps the states and saves them in the charxml (<mis><ts>), which the client reads on load.
	struct SetMissionTypeState : public NetGameMsg {
		SetMissionTypeState();
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		eMissionLockState state; // optional, default NEW
		std::string subtype{};
		std::string type{};
	};

	// Server -> client.
	struct NotifyLevelRewards : public NetGameMsg {
		NotifyLevelRewards() : NetGameMsg(MessageType::Game::NOTIFY_LEVEL_REWARDS) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		int32_t level{};
		bool sendingRewards{};
	};
};

#endif // MISSIONMESSAGES_H
