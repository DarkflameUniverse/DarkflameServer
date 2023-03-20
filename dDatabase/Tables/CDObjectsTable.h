#pragma once

// Custom Classes
#include "CDTable.h"

struct CDObjects {
	unsigned int id;                            //!< The LOT of the object
	std::string name;                      //!< The internal name of the object
	UNUSED_COLUMN(unsigned int placeable);                     //!< Whether or not the object is placable
	std::string type;                      //!< The object type
	UNUSED_COLUMN(std::string description);               //!< An internal description of the object
	UNUSED_COLUMN(unsigned int localize);                      //!< Whether or not the object should localize
	UNUSED_COLUMN(unsigned int npcTemplateID);                 //!< Something related to NPCs...
	UNUSED_COLUMN(std::string displayName);               //!< The display name of the object
	float interactionDistance;          //!< The interaction distance of the object
	UNUSED_COLUMN(unsigned int nametag);                       //!< ???
	UNUSED_COLUMN(std::string _internalNotes);            //!< Some internal notes (rarely used)
	UNUSED_COLUMN(unsigned int locStatus);                     //!< ???
	UNUSED_COLUMN(std::string gate_version);              //!< The gate version for the object
	UNUSED_COLUMN(unsigned int HQ_valid);                      //!< Probably used for the Nexus HQ database on LEGOUniverse.com
};

class CDObjectsTable : public CDTable<CDObjectsTable> {
private:
	std::map<unsigned int, CDObjects> entries;
	CDObjects m_default;

public:
	CDObjectsTable();
	// Gets an entry by ID
	const CDObjects& GetByID(unsigned int LOT);
};

