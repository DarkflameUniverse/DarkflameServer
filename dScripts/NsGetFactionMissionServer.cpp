#include "NsGetFactionMissionServer.h"
#include "GameMessages.h"
#include "MissionComponent.h"
#include "Character.h"

void NsGetFactionMissionServer::OnRespondToMission(Entity* self, int missionID, Entity* player, int reward) {
	if (missionID != 474) return;

	if (reward != LOT_NULL) {
		std::vector<int> factionMissions;
		int celebrationID = -1;
		int flagID = -1;

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
			player->GetCharacter()->SetPlayerFlag(ePlayerFlags::JOINED_A_FACTION, true);
			player->GetCharacter()->SetPlayerFlag(flagID, true);
		}

		MissionComponent* mis = static_cast<MissionComponent*>(player->GetComponent(COMPONENT_TYPE_MISSION));

		for (int mission : factionMissions) {
			mis->AcceptMission(mission);
			mis->CompleteMission(mission);
		}
	}
}
