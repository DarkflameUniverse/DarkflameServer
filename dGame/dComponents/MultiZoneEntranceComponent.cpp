#include "MultiZoneEntranceComponent.h"
#include "ObjectMessages.h"
#include "PropertyMessages.h"
#include "RocketLaunchpadControlComponent.h"
#include "InventoryComponent.h"
#include "CharacterComponent.h"

MultiZoneEntranceComponent::MultiZoneEntranceComponent(Entity* parent, const int32_t componentID) : Component(parent, componentID) {
	RegisterMsg(&MultiZoneEntranceComponent::OnRequestUse);
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

bool MultiZoneEntranceComponent::OnRequestUse(GameMessages::RequestUseEvent& event) {
	if (event.msg.IsMissionChoice()) return false;
	auto* const originator = event.user;
	auto* const characterComponent = originator->GetComponent<CharacterComponent>();
	if (!characterComponent) return true;

	auto* rocket = characterComponent->RocketEquip(originator);
	if (!rocket) return true;

	// the LUP world menu is just the property menu, the client knows how to handle it
	GameMessages::PropertyEntranceBegin msg;
	msg.target = m_Parent->GetObjectID();
	msg.Send(originator->GetSystemAddress());
	return true;
}

void MultiZoneEntranceComponent::OnSelectWorld(Entity* originator, uint32_t index) {
	auto* rocketLaunchpadControlComponent = m_Parent->GetComponent<RocketLaunchpadControlComponent>();
	if (!rocketLaunchpadControlComponent || index >= m_LUPWorlds.size()) return;

	rocketLaunchpadControlComponent->Launch(originator, m_LUPWorlds[index], 0, static_cast<int32_t>(index));
}
