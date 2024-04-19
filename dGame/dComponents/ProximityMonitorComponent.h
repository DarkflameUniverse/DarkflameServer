/*
 * Darkflame Universe
 * Copyright 2018
 */

#ifndef PROXIMITYMONITORCOMPONENT_H
#define PROXIMITYMONITORCOMPONENT_H

#include <unordered_set>

#include "BitStream.h"
#include "Entity.h"
#include "dpWorld.h"
#include "dpEntity.h"
#include "Component.h"
#include "eReplicaComponentType.h"

 /**
  * Utility component for detecting how close entities are to named proximities for this entity. Allows you to store
  * proximity checks for multiple ojects.
  */
class ProximityMonitorComponent final : public Component {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::PROXIMITY_MONITOR;

	ProximityMonitorComponent(Entity* parentEntity, int smallRadius = -1, int largeRadius = -1);
	~ProximityMonitorComponent() override;
	void Update(float deltaTime) override;

	/**
	 * Creates an entry to check proximity for, given a name
	 * @param proxRadius the radius to use for the physics entity we use to detect proximity
	 * @param name the name of this check
	 */
	void SetProximityRadius(float proxRadius, const std::string& name);

	/**
	 * Creates an entry to check proximity for, given a name
	 * @param entity the physics entity to add to our proximity sensors
	 * @param name the name of this check
	 */
	void SetProximityRadius(dpEntity* entity, const std::string& name);

	/**
	 * Returns the last of entities that are used to check proximity, given a name
	 * @param name the proximity name to retrieve physics objects for
	 * @return a set of physics entity object IDs for this name
	 */
	const std::unordered_set<LWOOBJID>& GetProximityObjects(const std::string& name);

	/**
	 * Checks if the passed object is in proximity of the named proximity sensor
	 * @param name the name of the sensor to check proximity for
	 * @param objectID the entity to check if they're in proximity
	 * @return true if the object is in proximity, false otherwise
	 */
	bool IsInProximity(const std::string& name, LWOOBJID objectID);

	/**
	 * Returns all the proximity sensors stored on this component, indexed by name
	 * @return all the proximity sensors stored on this component
	 */
	const std::map<std::string, dpEntity*>& GetProximitiesData() const { return m_ProximitiesData; }

private:

	/**
	 * All the proximity sensors for this component, indexed by name
	 */
	std::map<std::string, dpEntity*> m_ProximitiesData = {};

	/**
	 * Default value for the proximity data
	 */
	static const std::unordered_set<LWOOBJID> m_EmptyObjectSet;
};

#endif // PROXIMITYMONITORCOMPONENT_H
