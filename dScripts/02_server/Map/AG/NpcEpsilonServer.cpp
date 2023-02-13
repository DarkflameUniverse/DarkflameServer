#include "NpcEpsilonServer.h"
#include "GameMessages.h"

void NpcEpsilonServer::OnMissionDialogueOK(Entity* self, Entity* target, int missionID, eMissionState missionState) {

	//If we are completing the Nexus Force join mission, play the celebration for it:
	if (missionID == 1851) {
		GameMessages::SendStartCelebrationEffect(target, target->GetSystemAddress(), 22);
	}
}
