#include "NPCAddRemoveItem.h"
#include "InventoryComponent.h"

void NPCAddRemoveItem::OnMissionDialogueOK(Entity* self, Entity* target, int missionID, MissionState missionState) {
	auto* inventory = target->GetComponent<InventoryComponent>();
	if (inventory == nullptr)
		return;

	for (const auto& missionSetting : m_MissionItemSettings) {
		if (missionSetting.first == missionID) {
			for (const auto& itemSetting : missionSetting.second) {
				for (const auto& lot : itemSetting.items) {
					if (itemSetting.add && (missionState == MissionState::MISSION_STATE_AVAILABLE || missionState == MissionState::MISSION_STATE_COMPLETE_AVAILABLE)) {
						inventory->AddItem(lot, 1, eLootSourceType::LOOT_SOURCE_NONE);
					} else if (itemSetting.remove && (missionState == MissionState::MISSION_STATE_READY_TO_COMPLETE || missionState == MissionState::MISSION_STATE_COMPLETE_READY_TO_COMPLETE)) {
						inventory->RemoveItem(lot, 1);
					}
				}
			}
		}
	}
}

std::map<uint32_t, std::vector<ItemSetting>> NPCAddRemoveItem::GetSettings() {
	return std::map<uint32_t, std::vector<ItemSetting>>();
}

void NPCAddRemoveItem::OnStartup(Entity* self) {
	m_MissionItemSettings = GetSettings();
}
