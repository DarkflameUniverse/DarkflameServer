#include "NsGetFactionMissionServer.h"

#include "GameMessages.h"
#include "MissionComponent.h"
#include "eReplicaComponentType.h"
#include "ePlayerFlag.h"

void NsGetFactionMissionServer::OnRespondToMission(Entity* self, int missionID, Entity* player, int reward) {
	if (missionID != 474) return;

	if (reward != LOT_NULL) {
		std::vector<int> factionMissions;
		int celebrationID = -1;
		int32_t flagID = -1;

		if (reward == 6980) {
			// Venture League
			factionMissions = { 555, 556 };
			celebrationID = 14;
			flagID = 46;
		} else if (reward == 6979) {
			// Assembly
			factionMissions = { 544, 545 };
			celebrationID = 15;
			flagID = 47;
		} else if (reward == 6981) {
			// Paradox
			factionMissions = { 577, 578 };
			celebrationID = 16;
			flagID = 48;
		} else if (reward == 6978) {
			// Sentinel
			factionMissions = { 566, 567 };
			celebrationID = 17;
			flagID = 49;
		}

		factionMissions.push_back(778);

		if (celebrationID != -1) {
			GameMessages::SendStartCelebrationEffect(player, player->GetSystemAddress(), celebrationID);
		}

		if (flagID != -1) {
			GameMessages::SetFlag setFlag{};
			setFlag.target = player->GetObjectID();
			setFlag.iFlagId = ePlayerFlag::JOINED_A_FACTION;
			setFlag.bFlag = true;
			SEND_ENTITY_MSG(setFlag);
			setFlag.iFlagId = flagID;
			setFlag.bFlag = true;
			SEND_ENTITY_MSG(setFlag);
		}

		auto* mis = player->GetComponent<MissionComponent>();

		for (int mission : factionMissions) {
			mis->AcceptMission(mission);
			mis->CompleteMission(mission);
		}
	}
}
