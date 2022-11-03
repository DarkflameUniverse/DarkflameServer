#include "NjJayMissionItems.h"

void NjJayMissionItems::OnMissionDialogueOK(Entity* self, Entity* target, int missionID, MissionState missionState) {
	NjNPCMissionSpinjitzuServer::OnMissionDialogueOK(self, target, missionID, missionState);
	NPCAddRemoveItem::OnMissionDialogueOK(self, target, missionID, missionState);
}

std::map<uint32_t, std::vector<ItemSetting>> NjJayMissionItems::GetSettings() {
	return {
		{1789, {{{14474},false, true}}},
		{1927, {{{14493},false, true}}}
	};
}
