#pragma once

// Custom Classes
#include "CDTable.h"

struct CDScriptComponent {
	std::string script_name;           		//!< The script name
	std::string client_script_name;   		//!< The client script name
};

class CDScriptComponentTable : public CDTable<CDScriptComponentTable> {
private:
	std::unordered_map<unsigned int, CDScriptComponent> entries;
public:
	void LoadValuesFromDatabase();
	// Gets an entry by scriptID
	const std::optional<CDScriptComponent> GetByID(unsigned int id);
};

