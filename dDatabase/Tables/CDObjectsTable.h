#pragma once

// Custom Classes
#include "CDTable.h"

struct CDObjects {
	unsigned int id;                            //!< The LOT of the object
	std::string name;                      //!< The internal name of the object
	UNUSED(unsigned int placeable);                     //!< Whether or not the object is placable
	std::string type;                      //!< The object type
	UNUSED(std::string description);               //!< An internal description of the object
	UNUSED(unsigned int localize);                      //!< Whether or not the object should localize
	UNUSED(unsigned int npcTemplateID);                 //!< Something related to NPCs...
	UNUSED(std::string displayName);               //!< The display name of the object
	float interactionDistance;          //!< The interaction distance of the object
	UNUSED(unsigned int nametag);                       //!< ???
	UNUSED(std::string _internalNotes);            //!< Some internal notes (rarely used)
	UNUSED(unsigned int locStatus);                     //!< ???
	UNUSED(std::string gate_version);              //!< The gate version for the object
	UNUSED(unsigned int HQ_valid);                      //!< Probably used for the Nexus HQ database on LEGOUniverse.com
};

class CDObjectsTable : public CDTable<CDObjectsTable> {
private:
	std::map<unsigned int, CDObjects> entries;
	CDObjects m_default;

public:
	void LoadValuesFromDatabase();
	// Gets an entry by ID
	const CDObjects& GetByID(unsigned int LOT);
};

