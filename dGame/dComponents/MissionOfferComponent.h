/*
 * Darkflame Universe
 * Copyright 2019
 */

#ifndef MISSIONOFFERCOMPONENT_H
#define MISSIONOFFERCOMPONENT_H

#include "dCommonVars.h"
#include "Component.h"
#include <optional>
#include <string>
#include <vector>
#include <stdint.h>
#include "eReplicaComponentType.h"

class Entity;
class MissionComponent;

// One mission of a mission giver for a player, as the client lists them (LWOMissionOfferComponent 0x00c605f0): the
// lowest priority comes first. 0 a mission to turn in here, 5 one in progress, 10 a new one, 15 a repeat, 20 a
// completed one.
struct MissionOfferEntry {
	uint8_t priority{};
	uint32_t missionID{};
	int32_t state{};
	bool isTurnIn{};

	bool operator==(const MissionOfferEntry&) const = default;
};

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
	bool GetOffersMission() const;

	/**
	 * Returns if this mission may be accepted by the entity (currently unused)
	 * @return true if this mission may be accepted by the entity, false otherwise
	 */
	bool GetAcceptsMission() const;

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
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::MISSION_OFFER;

	MissionOfferComponent(Entity* parent, const int32_t componentID);

	void LoadDataFromTemplate() override;

	/**
	 * Handles a RequestUse triggered by some entity, determines which missions to show based on what they may
	 * hand in now and what they may start based on their mission history.
	 * @param originator the entity that triggered the event
	 */
	bool OnRequestUse(GameMessages::RequestUseEvent& event);

	/**
	 * Offers all the missions an entity can accept to said entity
	 * @param entity the entity to offer missions to
	 * @param specifiedMissionId optional mission ID if you wish to offer a specific mission
	 */
	void OfferMissions(Entity* entity, uint32_t specifiedMissionId = 0);

	/**
	 * Returns whether this entity offers or accepts a mission, on its own or as one of a random mission's pool
	 * @param missionId the mission to look for
	 */
	bool HasMission(uint32_t missionId) const;

	// RequestLinkedMission after a mission window: offers the next mission of this giver, if there is a new one
	void OfferLinkedMission(Entity* entity, uint32_t missionID);

	// The client's entry for one mission (0x00bebf30), from its state bit mask (MissionComponent::GetClientMissionState)
	static std::optional<MissionOfferEntry> GetOfferEntry(uint32_t missionID, int32_t state, bool cooldownRunning, bool offers, bool accepts, bool alsoCompleted);

	// Inserts after the entries of the same priority, as the client's multimap does
	static void InsertByPriority(std::vector<MissionOfferEntry>& entries, const MissionOfferEntry& entry);

	// The missions of this giver for a player, in the client's order (0x00c605f0)
	std::vector<MissionOfferEntry> GetMissionsForPlayer(const MissionComponent& missionComponent, bool alsoCompleted) const;

	// The mission a RequestLinkedMission offers: the head of the list without the mission the window was for, when new
	static std::optional<uint32_t> GetLinkedMission(std::vector<MissionOfferEntry> entries, uint32_t missionID);

	static std::vector<uint32_t> ParseRandomPool(const std::string& randomPool);

private:
	const OfferedMission* GetOfferedMission(uint32_t missionID) const;
	static std::optional<MissionOfferEntry> GetEntryFor(const MissionComponent& missionComponent, const OfferedMission& offered, bool alsoCompleted);

	/**
	 * The missions this entity has to offer
	 */
	std::vector<OfferedMission> offeredMissions;
};

#endif // MISSIONOFFERCOMPONENT_H
