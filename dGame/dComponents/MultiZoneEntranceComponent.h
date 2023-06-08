#pragma once

#include "Entity.h"
#include "GameMessages.h"
#include "Component.h"
#include "eReplicaComponentType.h"

/**
 * Component that handles the LUP/WBL rocket launchpad that can be interacted with to travel to WBL worlds.
  *
 */
class MultiZoneEntranceComponent : public Component {
public:
	static const eReplicaComponentType ComponentType = eReplicaComponentType::MULTI_ZONE_ENTRANCE;

	/**
	 * Constructor for this component, builds the m_LUPWorlds vector
	 * @param parent parent that contains this component
	 */
	MultiZoneEntranceComponent(Entity* parent);
	~MultiZoneEntranceComponent() override;

	/**
	 * Handles an OnUse event from some entity, preparing it for launch to some other world
	 * @param originator the entity that triggered the event
	 */
	void OnUse(Entity* originator) override;

	/**
	 * Handles an OnUse event from some entity, preparing it for launch to some other world
	 * @param originator the entity that triggered the event
	 * @param index index of the world that was selected
	 */
	void OnSelectWorld(Entity* originator, uint32_t index);
private:
	/**
	 * vector of the LUP World Zone IDs, built from CDServer's LUPZoneIDs table
	 */
	std::vector<LWOMAPID> m_LUPWorlds{};
};
