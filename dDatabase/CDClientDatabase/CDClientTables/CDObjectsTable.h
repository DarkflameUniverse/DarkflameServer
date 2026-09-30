#pragma once

// Custom Classes
#include "CDTable.h"

#include <cstdint>

#include "FdbReader.h"

struct CDObjects {
	uint32_t id;                            //!< The LOT of the object
	std::string name;                      //!< The internal name of the object
	UNUSED(uint32_t placeable);                     //!< Whether or not the object is placable
	std::string type;                      //!< The object type
	UNUSED(std::string description);               //!< An internal description of the object
	UNUSED(uint32_t localize);                      //!< Whether or not the object should localize
	UNUSED(uint32_t npcTemplateID);                 //!< Something related to NPCs...
	UNUSED(std::string displayName);               //!< The display name of the object
	float interactionDistance;          //!< The interaction distance of the object
	UNUSED(uint32_t nametag);                       //!< ???
	UNUSED(std::string _internalNotes);            //!< Some internal notes (rarely used)
	UNUSED(uint32_t locStatus);                     //!< ???
	UNUSED(std::string gate_version);              //!< The gate version for the object
	UNUSED(uint32_t HQ_valid);                      //!< Probably used for the Nexus HQ database on LEGOUniverse.com
};

class CDObjectsTable : public CDTable<CDObjectsTable, std::map<uint32_t, CDObjects>> {
public:
	void LoadValuesFromDatabase();

	// Reads rows from the client's fdb from now on, if it is open; false keeps the table on CDServer.sqlite
	bool LoadFromFdb();

	// Gets an entry by ID
	const CDObjects& GetByID(const uint32_t lot);

private:
	// Caches the entry of one id from CDServer.sqlite (the default entry when there is none) and returns it
	const CDObjects& LoadFromSqlite(const uint32_t lot);

	const FdbReader::Table* m_FdbTable = nullptr;
};

