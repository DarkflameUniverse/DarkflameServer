/*
 * Darkflame Universe
 * Copyright 2023
 */

#ifndef __MISSIONOFFERCOMPONENT_H__
#define __MISSIONOFFERCOMPONENT_H__
#pragma once

#include <memory>
#include <vector>
#include <stdint.h>

#include "dCommonVars.h"
#include "Component.h"
#include "eReplicaComponentType.h"

class Entity;

/**
 * Light wrapper around missions that may be offered by an entity
 */
struct OfferedMission {
	OfferedMission(const uint32_t missionId, const bool offersMission, const bool acceptsMission) {
		this->missionId = missionId;
		this->offersMission = offersMission;
		this->acceptsMission = acceptsMission;
	};

	/**
	 * Returns the ID of the mission
	 * @return the ID of the mission
	 */
	uint32_t GetMissionId() const { return missionId; };

	/**
	 * Returns if this mission is offered by the entity
	 * @return true if this mission is offered by the entity, false otherwise
	 */
	bool GetOfferMission() const { return offersMission; };

	/**
	 * Returns if this mission may be accepted by the entity (currently unused)
	 * @return true if this mission may be accepted by the entity, false otherwise
	 */
	bool GetAcceptMission() const { return acceptsMission; };

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
class MissionOfferComponent final : public Component {
public:
	inline static const eReplicaComponentType ComponentType = eReplicaComponentType::MISSION_OFFER;

	MissionOfferComponent(Entity* parent, const int32_t componentId = -1);

	void LoadTemplateData() override;

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
	std::vector<std::unique_ptr<OfferedMission>> offeredMissions;

	int32_t m_ComponentId;
};

#endif // __MISSIONOFFERCOMPONENT_H__
