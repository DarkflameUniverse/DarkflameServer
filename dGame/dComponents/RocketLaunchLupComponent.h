#pragma once

#include "Entity.h"
#include "GameMessages.h"
#include "Component.h"
#include "Item.h"

/**
 * Component that handles the LUP/WBL rocket launchpad that can be interacted with to travel to WBL worlds.
  *
 */
class RocketLaunchLupComponent : public Component {
public:
	static const uint32_t ComponentType = eReplicaComponentType::COMPONENT_TYPE_ROCKET_LAUNCH_LUP;

	RocketLaunchLupComponent(Entity* parent);
	~RocketLaunchLupComponent() override;

	void OnUse(Entity* originator) override;

	void OnSelectWorld(Entity* originator, uint32_t index, const SystemAddress& sysAddr);
private:
	std::vector<LWOMAPID> m_LUPWorlds {};
	Item* m_rocket = nullptr;
};
