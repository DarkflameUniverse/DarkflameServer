#pragma once
#include "AmTemplateSkillVolume.h"

class NjWuNPC : public AmTemplateSkillVolume {
	void OnMissionDialogueOK(Entity* self, Entity* target, int missionID, MissionState missionState) override;
	const uint32_t m_MainDragonMissionID = 2040;
	const std::vector<uint32_t> m_SubDragonMissionIDs = { 2064, 2065, 2066, 2067 };

	// Groups and variables
	const std::string m_DragonChestGroup = "DragonEmblemChest";
	const std::string m_DragonStatueGroup = "Minidragons";
	const std::u16string m_ShowChestNotification = u"showChest";
};
