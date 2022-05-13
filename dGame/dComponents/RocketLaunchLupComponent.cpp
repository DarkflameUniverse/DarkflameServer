#include "RocketLaunchLupComponent.h"
#include "CDClientDatabase.h"
#include "RocketLaunchpadControlComponent.h"
#include "InventoryComponent.h"
#include "CharacterComponent.h"

RocketLaunchLupComponent::RocketLaunchLupComponent(Entity* parent) : Component(parent) {
	m_Parent = parent;

	// get the lup worlds from the cdclient
	std::string query = "SELECT * FROM LUPZoneIDs;";
	auto results = CDClientDatabase::ExecuteQuery(query);
	while (!results.eof()) {
		// fallback to 1600 incase there is an issue
		m_LUPWorlds.push_back(results.getIntField(0, 1600));
		results.nextRow();
	}
	results.finalize();
}

RocketLaunchLupComponent::~RocketLaunchLupComponent() {}

void RocketLaunchLupComponent::OnUse(Entity* originator) {
	auto* rocket = originator->GetComponent<CharacterComponent>()->RocketEquip(originator);
	if (!rocket) return;

	// the LUP world menu is just the property menu, the client knows how to handle it
	GameMessages::SendPropertyEntranceBegin(m_Parent->GetObjectID(), m_Parent->GetSystemAddress());
}

void RocketLaunchLupComponent::OnSelectWorld(Entity* originator, uint32_t index) {
	// Add one to index because the actual LUP worlds start at index 1.
	index++;

	auto* rocketLaunchpadControlComponent = m_Parent->GetComponent<RocketLaunchpadControlComponent>();

	if (!rocketLaunchpadControlComponent) return;

	rocketLaunchpadControlComponent->Launch(originator, m_LUPWorlds[index], 0);
}
