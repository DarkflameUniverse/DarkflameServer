#pragma once

// Custom Classes
#include "CDTable.h"

/*!
 \file CDZoneTableTable.hpp
 \brief Contains data for the ZoneTable table
 */

 //! ZoneTable Struct
struct CDZoneTable {
	unsigned int zoneID;                        //!< The Zone ID of the object
	unsigned int locStatus;                     //!< The Locale Status(?)
	std::string zoneName;                  //!< The name of the zone
	unsigned int scriptID;                      //!< The Script ID of the zone (ScriptsTable)
	float ghostdistance_min;            //!< The minimum ghosting distance
	float ghostdistance;                //!< The ghosting distance
	unsigned int population_soft_cap;           //!< The "soft cap" on the world population
	unsigned int population_hard_cap;           //!< The "hard cap" on the world population
	UNUSED(std::string DisplayDescription);        //!< The display description of the world
	UNUSED(std::string mapFolder);                 //!< ???
	float smashableMinDistance;         //!< The minimum smashable distance?
	float smashableMaxDistance;         //!< The maximum smashable distance?
	UNUSED(std::string mixerProgram);              //!< ???
	UNUSED(std::string clientPhysicsFramerate);    //!< The client physics framerate
	UNUSED(std::string serverPhysicsFramerate);    //!< The server physics framerate
	unsigned int zoneControlTemplate;           //!< The Zone Control template
	unsigned int widthInChunks;                 //!< The width of the world in chunks
	unsigned int heightInChunks;                //!< The height of the world in chunks
	bool petsAllowed;                   //!< Whether or not pets are allowed in the world
	bool localize;                      //!< Whether or not the world should be localized
	float fZoneWeight;                  //!< ???
	UNUSED(std::string thumbnail);                 //!< The thumbnail of the world
	bool PlayerLoseCoinsOnDeath;        //!< Whether or not the user loses coins on death
	UNUSED(bool disableSaveLoc);                //!< Disables the saving location?
	float teamRadius;                   //!< ???
	UNUSED(std::string gate_version);              //!< The gate version
	UNUSED(bool mountsAllowed);                 //!< Whether or not mounts are allowed
};

//! ZoneTable table
class CDZoneTableTable : public CDTable {
private:
	std::map<unsigned int, CDZoneTable> m_Entries;

public:

	//! Constructor
	CDZoneTableTable(void);

	//! Destructor
	~CDZoneTableTable(void);

	//! Returns the table's name
	/*!
	 \return The table name
	 */
	std::string GetName(void) const override;

	//! Queries the table with a zoneID to find.
	/*!
	 \param id The zoneID
	 */
	const CDZoneTable* Query(unsigned int zoneID);
};
