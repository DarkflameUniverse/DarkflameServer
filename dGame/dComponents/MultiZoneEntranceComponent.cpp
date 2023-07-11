#include "MultiZoneEntranceComponent.h"

#include "RocketLaunchpadControlComponent.h"
#include "InventoryComponent.h"
#include "CharacterComponent.h"
#include "GameMessages.h"

void MultiZoneEntranceComponent::LoadConfigData() {
	std::string zoneString = GeneralUtils::UTF16ToWTF8(m_ParentEntity->GetVar<std::u16string>(u"MultiZoneIDs"));
	const auto zoneSplitStr = GeneralUtils::SplitString(zoneString, ';');
	for (const auto& zone : zoneSplitStr) {
		uint32_t mapId;
		if (GeneralUtils::TryParse(zone, mapId)) m_LUPWorlds.push_back(mapId);
	}
}

void MultiZoneEntranceComponent::OnUse(Entity* originator) {
	auto* characterComponent = originator->GetComponent<CharacterComponent>();
	if (!characterComponent) return;
	auto* rocket = characterComponent->RocketEquip(originator);
	if (!rocket) return;

	// The LUP world menu is just the property menu, the client handles this in flash
	GameMessages::SendPropertyEntranceBegin(m_ParentEntity->GetObjectID(), m_ParentEntity->GetSystemAddress());
}

void MultiZoneEntranceComponent::OnSelectWorld(Entity* originator, const uint32_t index) const {
	auto* rocketLaunchpadControlComponent = m_ParentEntity->GetComponent<RocketLaunchpadControlComponent>();
	if (!rocketLaunchpadControlComponent || index >= m_LUPWorlds.size()) return;

	rocketLaunchpadControlComponent->Launch(originator, m_LUPWorlds[index], 0);
}
