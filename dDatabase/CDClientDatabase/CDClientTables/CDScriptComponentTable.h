#pragma once

// Custom Classes
#include "CDTable.h"

struct CDScriptComponent {
	uint32_t id;                        //!< The component ID
	std::string script_name;           //!< The script name
	std::string client_script_name;    //!< The client script name
};

class CDScriptComponentTable : public CDTable<CDScriptComponentTable, std::map<uint32_t, CDScriptComponent>> {
public:
	void LoadValuesFromDatabase();
	// Gets an entry by scriptID
	const CDScriptComponent& GetByID(uint32_t id);
};

