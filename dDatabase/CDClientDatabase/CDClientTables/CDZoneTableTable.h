#pragma once

// Custom Classes
#include "CDTable.h"

struct CDZoneTable {
	uint32_t zoneID;                        //!< The Zone ID of the object
	uint32_t locStatus;                     //!< The Locale Status(?)
	std::string zoneName;                  //!< The name of the zone
	uint32_t scriptID;                      //!< The Script ID of the zone (ScriptsTable)
	float ghostdistance_min;            //!< The minimum ghosting distance
	float ghostdistance;                //!< The ghosting distance
	uint32_t population_soft_cap;           //!< The "soft cap" on the world population
	uint32_t population_hard_cap;           //!< The "hard cap" on the world population
	UNUSED(std::string DisplayDescription);        //!< The display description of the world
	UNUSED(std::string mapFolder);                 //!< ???
	float smashableMinDistance;         //!< The minimum smashable distance?
	float smashableMaxDistance;         //!< The maximum smashable distance?
	UNUSED(std::string mixerProgram);              //!< ???
	UNUSED(std::string clientPhysicsFramerate);    //!< The client physics framerate
	std::string serverPhysicsFramerate;    //!< The server physics framerate
	uint32_t zoneControlTemplate;           //!< The Zone Control template
	uint32_t widthInChunks;                 //!< The width of the world in chunks
	uint32_t heightInChunks;                //!< The height of the world in chunks
	bool petsAllowed;                   //!< Whether or not pets are allowed in the world
	bool localize;                      //!< Whether or not the world should be localized
	float fZoneWeight;                  //!< ???
	UNUSED(std::string thumbnail);                 //!< The thumbnail of the world
	bool PlayerLoseCoinsOnDeath;        //!< Whether or not the user loses coins on death
	bool disableSaveLoc;                //!< Disables the saving location?
	float teamRadius;                   //!< ???
	UNUSED(std::string gate_version);              //!< The gate version
	bool mountsAllowed;                 //!< Whether or not mounts are allowed
};

class CDZoneTableTable : public CDTable<CDZoneTableTable> {
private:
	std::map<uint32_t, CDZoneTable> m_Entries;

public:
	void LoadValuesFromDatabase();

	// Queries the table with a zoneID to find.
	const CDZoneTable* Query(uint32_t zoneID);
};
