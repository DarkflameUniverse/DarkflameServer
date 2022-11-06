/*
 * Darkflame Universe
 * Copyright 2019
 */

#ifndef MISSIONOFFERCOMPONENT_H
#define MISSIONOFFERCOMPONENT_H

#include "dCommonVars.h"
#include "Component.h"
#include <vector>
#include <stdint.h>

class Entity;

/**
 * Light wrapper around missions that may be offered by an entity
 */
struct OfferedMission {
	OfferedMission(uint32_t missionId, bool offersMission, bool acceptsMission);

	/**
	 * Returns the ID of the mission
	 * @return the ID of the mission
	 */
	uint32_t GetMissionId() const;

	/**
	 * Returns if this mission is offered by the entity
	 * @return true if this mission is offered by the entity, false otherwise
	 */
	bool GetOfferMission() const;

	/**
	 * Returns if this mission may be accepted by the entity (currently unused)
	 * @return true if this mission may be accepted by the entity, false otherwise
	 */
	bool GetAcceptMission() const;

private:

	/**
	 * The ID of the mission
	 */
	uint32_t missionId;

	/**
	 * Determines if the mission is offered by the entity
	 */
	bool offersMission;

	/**
	 * Determines if the mission can be accepted by the entity
	 */
	bool acceptsMission;
};

/**
 * Allows entities to offer missions to other entities, depending on their mission inventory progression.
 */
class MissionOfferComponent : public Component {
public:
	static const uint32_t ComponentType = COMPONENT_TYPE_MISSION_OFFER;

	MissionOfferComponent(Entity* parent, LOT parentLot);
	~MissionOfferComponent() override;

	/**
	 * Handles the OnUse event triggered by some entity, determines which missions to show based on what they may
	 * hand in now and what they may start based on their mission history.
	 * @param originator the entity that triggered the event
	 */
	void OnUse(Entity* originator) override;

	/**
	 * Offers all the missions an entity can accept to said entity
	 * @param entity the entity to offer missions to
	 * @param specifiedMissionId optional mission ID if you wish to offer a specific mission
	 */
	void OfferMissions(Entity* entity, uint32_t specifiedMissionId = 0);

private:

	/**
	 * The missions this entity has to offer
	 */
	std::vector<OfferedMission*> offeredMissions;
};

#endif // MISSIONOFFERCOMPONENT_H
