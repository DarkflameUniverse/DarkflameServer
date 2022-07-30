#pragma once
#include "CppScripts.h"

struct ItemSetting {
	std::vector<LOT> items; // The items to add/remove
	bool add;               // Add items on mission accept
	bool remove;            // Remove items on mission complete
};

/**
 * Scripts that allows for the addition and removal of sets of items by an NPC
 */
class NPCAddRemoveItem : public CppScripts::Script {
protected:
	void OnMissionDialogueOK(Entity* self, Entity* target, int missionID, MissionState missionState) override;
	virtual std::map<uint32_t, std::vector<ItemSetting>> GetSettings();
private:
	void OnStartup(Entity* self) override;
	std::map<uint32_t, std::vector<ItemSetting>> m_MissionItemSettings;
};
