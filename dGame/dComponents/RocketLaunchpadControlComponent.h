/*
 * Darkflame Universe
 * Copyright 2018
 */

#ifndef ROCKETLAUNCHPADCONTROLCOMPONENT_H
#define ROCKETLAUNCHPADCONTROLCOMPONENT_H

#include "BitStream.h"
#include "Entity.h"
#include "Component.h"

class PreconditionExpression;

/**
 * Component that handles rocket launchpads that can be interacted with to travel to other worlds.
 */
class RocketLaunchpadControlComponent : public Component {
public:
	static const uint32_t ComponentType = COMPONENT_TYPE_ROCKET_LAUNCH;

	RocketLaunchpadControlComponent(Entity* parent, int rocketId);
	~RocketLaunchpadControlComponent() override;

	/**
	 * Launches some entity to another world
	 * @param originator the entity to launch
	 * @param mapId the world to go to
	 * @param cloneId the clone ID (for properties)
	 */
	void Launch(Entity* originator, LWOMAPID mapId = LWOMAPID_INVALID, LWOCLONEID cloneId = LWOCLONEID_INVALID);

	/**
	 * Handles an OnUse event from some entity, preparing it for launch to some other world
	 * @param originator the entity that triggered the event
	 */
	void OnUse(Entity* originator) override;

	/**
	 * Currently unused
	 */
	void OnProximityUpdate(Entity* entering, std::string name, std::string status);

	/**
	 * Sets the map ID that a player will go to
	 * @param player the entity to set the map ID for
	 * @param cloneId the map ID of the property to set
	 */
	void SetSelectedMapId(LWOOBJID player, LWOMAPID cloneId);

	/**
	 * Returns the map ID that a player will go to
	 * @param player the player to find the map ID for
	 * @return the map ID that a player will go to
	 */
	LWOMAPID GetSelectedMapId(LWOOBJID player) const;

	/**
	 * Sets the clone ID that a player will go to (for properties)
	 * @param player the entity to set the clone ID for
	 * @param cloneId the clone ID of the property to set
	 */
	void SetSelectedCloneId(LWOOBJID player, LWOCLONEID cloneId);

	/**
	 * Returns the clone ID that a player will go to (for properties)
	 * @param player the player to find the clone ID for
	 * @return the clone ID that a player will go to
	 */
	LWOCLONEID GetSelectedCloneId(LWOOBJID player) const;

	/**
	 * Returns the zone that this rocket launchpad points to by default
	 * @return the zone that this rocket launchpad points to by default
	 */
	LWOMAPID GetTargetZone() const;

	/**
	 * Currently unused
	 */
	LWOMAPID GetDefaultZone() const;

private:

	/**
	 * All the players that are in the proximity of the rocket launchpad
	 */
	std::map<LWOOBJID, Entity*> m_PlayersInRadius = {};

	/**
	 * The map that the launchpad goes to
	 */
	LWOMAPID m_TargetZone;

	/**
	 * Currently unused
	 */
	LWOMAPID m_DefaultZone;

	/**
	 * The clone IDs selected for each player to go to (for properies)
	 */
	std::map<LWOOBJID, LWOCLONEID> m_SelectedCloneIds = {};

	/**
	 * The map IDs selected for each player to go to
	 */
	std::map<LWOOBJID, LWOMAPID> m_SelectedMapIds = {};

	/**
	 * The scene that plays when the player lands
	 */
	std::string m_TargetScene;

	/**
	 * Alternative landing scene that plays if the alternative precondition is met
	 */
	std::string m_AltLandingScene;

	/**
	 * Some precondition that needs to be met to trigger the alternative landing scene
	 */
	PreconditionExpression* m_AltPrecondition;

	/**
	 * Notifies the master server to prepare some world for a player to be able to travel to it
	 * @param zoneID the ID of the zone to prepare
	 */
	void TellMasterToPrepZone(int zoneID);
};

#endif // ROCKETLAUNCHPADCONTROLCOMPONENT_H
