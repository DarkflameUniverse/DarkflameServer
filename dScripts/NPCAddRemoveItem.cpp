#include "NPCAddRemoveItem.h"
#include "InventoryComponent.h"
#include "eMissionState.h"

void NPCAddRemoveItem::OnMissionDialogueOK(Entity* self, Entity* target, int missionID, eMissionState missionState) {
	auto* inventory = target->GetComponent<InventoryComponent>();
	if (inventory == nullptr)
		return;

	for (const auto& missionSetting : m_MissionItemSettings) {
		if (missionSetting.first == missionID) {
			for (const auto& itemSetting : missionSetting.second) {
				for (const auto& lot : itemSetting.items) {
					if (itemSetting.add && (missionState == eMissionState::AVAILABLE || missionState == eMissionState::COMPLETE_AVAILABLE)) {
						inventory->AddItem(lot, 1, eLootSourceType::NONE);
					} else if (itemSetting.remove && (missionState == eMissionState::READY_TO_COMPLETE || missionState == eMissionState::COMPLETE_READY_TO_COMPLETE)) {
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
