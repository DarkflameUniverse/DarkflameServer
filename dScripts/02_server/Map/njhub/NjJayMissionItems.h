#pragma once
#include "NjNPCMissionSpinjitzuServer.h"
#include "NPCAddRemoveItem.h"
#include <vector>
#include <map>

class NjJayMissionItems : public NjNPCMissionSpinjitzuServer, NPCAddRemoveItem {
	void OnMissionDialogueOK(Entity* self, Entity* target, int missionID, MissionState missionState) override;
	std::map<uint32_t, std::vector<ItemSetting>> GetSettings() override;
};
