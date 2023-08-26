#include "MultiZoneEntranceComponent.h"
#include "RocketLaunchpadControlComponent.h"
#include "InventoryComponent.h"
#include "CharacterComponent.h"

MultiZoneEntranceComponent::MultiZoneEntranceComponent(Entity* parent) : Component(parent) {
	m_Parent = parent;
	std::string zoneString = GeneralUtils::UTF16ToWTF8(m_Parent->GetVar<std::u16string>(u"MultiZoneIDs"));
	std::stringstream ss(zoneString);
	for (int i; ss >> i;) {
		m_LUPWorlds.push_back(i);
		if (ss.peek() == ';')
			ss.ignore();
	}
}

MultiZoneEntranceComponent::~MultiZoneEntranceComponent() {}

void MultiZoneEntranceComponent::OnUse(Entity* originator) {
	auto* rocket = originator->GetComponent<CharacterComponent>()->RocketEquip(originator);
	if (!rocket) return;

	// the LUP world menu is just the property menu, the client knows how to handle it
	GameMessages::SendPropertyEntranceBegin(m_Parent->GetObjectID(), m_Parent->GetSystemAddress());
}

void MultiZoneEntranceComponent::OnSelectWorld(Entity* originator, uint32_t index) {
	auto* rocketLaunchpadControlComponent = m_Parent->GetComponent<RocketLaunchpadControlComponent>();
	if (!rocketLaunchpadControlComponent) return;

	rocketLaunchpadControlComponent->Launch(originator, m_LUPWorlds[index], 0);
}
