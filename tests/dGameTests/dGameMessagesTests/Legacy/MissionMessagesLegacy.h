#ifndef MISSIONMESSAGESLEGACY_H
#define MISSIONMESSAGESLEGACY_H

// FROZEN ORACLE - DO NOT EDIT.
// Verbatim copies of the hand written GameMessages functions that MissionMessages.h replaced
// (dGame/dGameMessages/GameMessages.cpp, branched from origin/main 129199e4). Only the namespace changed.
// The Read* functions are the read sequences of the replaced GameMessages::Handle* functions.

#include "BitStreamUtils.h"
#include "dCommonVars.h"
#include "dServer.h"
#include "Entity.h"
#include "eMissionState.h"
#include "Game.h"
#include "MessageType/Client.h"
#include "MessageType/Game.h"
#include "ServiceType.h"

#include <vector>

namespace LegacyGameMessages {
	inline void SendResetMissions(Entity* entity, const SystemAddress& sysAddr, const int32_t missionid) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(entity->GetObjectID());
		bitStream.Write(MessageType::Game::RESET_MISSIONS);

		bitStream.Write(missionid != -1);
		if (missionid != -1) bitStream.Write(missionid);

		SEND_PACKET;
	}

	inline void SendNotifyClientFlagChange(const LWOOBJID& objectID, uint32_t iFlagID, bool bFlag, const SystemAddress& sysAddr) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(objectID);
		bitStream.Write(MessageType::Game::NOTIFY_CLIENT_FLAG_CHANGE);
		bitStream.Write(bFlag);
		bitStream.Write(iFlagID);

		SEND_PACKET;
	}

	inline void SendOfferMission(const LWOOBJID& entity, const SystemAddress& sysAddr, int32_t missionID, const LWOOBJID& offererID) {
		//You might be wondering.
		//"Why are we sending it twice, once to a non-player object?
		//Well, the first one (sent to the offerer) makes the client zoom into the object.
		//The second, actually makes the UI pop up so you can be offered the mission.
		//Why is it like this? Because LU isn't just a clown, it's the entire circus.

		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(offererID);
		bitStream.Write(MessageType::Game::OFFER_MISSION);
		bitStream.Write(missionID);
		bitStream.Write(offererID);

		SEND_PACKET;

		{
			CBITSTREAM;
			CMSGHEADER;

			bitStream.Write(entity);
			bitStream.Write(MessageType::Game::OFFER_MISSION);
			bitStream.Write(missionID);
			bitStream.Write(offererID);

			SEND_PACKET;
		}
	}

	inline void SendNotifyMission(Entity* entity, const SystemAddress& sysAddr, int missionID, int missionState, bool sendingRewards) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(entity->GetObjectID());
		bitStream.Write(MessageType::Game::NOTIFY_MISSION);
		bitStream.Write(missionID);
		bitStream.Write(missionState);
		bitStream.Write(sendingRewards);

		SEND_PACKET;
	}

	inline void SendNotifyMissionTask(Entity* entity, const SystemAddress& sysAddr, int missionID, int taskMask, std::vector<float> updates) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(entity->GetObjectID());
		bitStream.Write(MessageType::Game::NOTIFY_MISSION_TASK);

		bitStream.Write(missionID);
		bitStream.Write(taskMask);
		bitStream.Write<unsigned char>(updates.size());

		for (uint32_t i = 0; i < updates.size(); ++i) {
			bitStream.Write(updates[i]);
		}

		SEND_PACKET;
	}

	inline void NotifyLevelRewards(LWOOBJID objectID, const SystemAddress& sysAddr, int level, bool sending_rewards) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(objectID);
		bitStream.Write(MessageType::Game::NOTIFY_LEVEL_REWARDS);

		bitStream.Write(level);
		bitStream.Write(sending_rewards);

		SEND_PACKET;
	}

	// GameMessages::HandleSetFlag
	struct LegacySetFlag { bool bFlag{}; int32_t iFlagID{}; };
	inline LegacySetFlag ReadSetFlag(RakNet::BitStream& inStream) {
		bool bFlag{};
		int32_t iFlagID{};

		inStream.Read(bFlag);
		inStream.Read(iFlagID);
		return { bFlag, iFlagID };
	}

	// GameMessages::HandleRespondToMission
	struct LegacyRespondToMission { int missionID{}; LWOOBJID playerID{}; LWOOBJID receiverID{}; LOT reward = LOT_NULL; };
	inline LegacyRespondToMission ReadRespondToMission(RakNet::BitStream& inStream) {
		int missionID{};
		LWOOBJID playerID{};
		LWOOBJID receiverID{};
		bool isDefaultReward{};
		LOT reward = LOT_NULL;

		inStream.Read(missionID);
		inStream.Read(playerID);
		inStream.Read(receiverID);
		inStream.Read(isDefaultReward);
		if (isDefaultReward) inStream.Read(reward);
		return { missionID, playerID, receiverID, reward };
	}

	// GameMessages::HandleMissionDialogOK
	struct LegacyMissionDialogOK { bool bIsComplete{}; eMissionState iMissionState{}; int missionID{}; LWOOBJID responder{}; };
	inline LegacyMissionDialogOK ReadMissionDialogOK(RakNet::BitStream& inStream) {
		bool bIsComplete{};
		eMissionState iMissionState{};
		int missionID{};
		LWOOBJID responder{};

		inStream.Read(bIsComplete);
		inStream.Read(iMissionState);
		inStream.Read(missionID);
		inStream.Read(responder);
		return { bIsComplete, iMissionState, missionID, responder };
	}

	// GameMessages::HandleRequestLinkedMission
	struct LegacyRequestLinkedMission { LWOOBJID playerId{}; int missionId{}; bool bMissionOffered{}; };
	inline LegacyRequestLinkedMission ReadRequestLinkedMission(RakNet::BitStream& inStream) {
		LWOOBJID playerId{};
		int missionId{};
		bool bMissionOffered{};

		inStream.Read(playerId);
		inStream.Read(missionId);
		inStream.Read(bMissionOffered);
		return { playerId, missionId, bMissionOffered };
	}

	// GameMessages::HandleHasBeenCollected
	inline LWOOBJID ReadHasBeenCollected(RakNet::BitStream& inStream) {
		LWOOBJID playerID;
		inStream.Read(playerID);
		return playerID;
	}
}

#endif // MISSIONMESSAGESLEGACY_H
