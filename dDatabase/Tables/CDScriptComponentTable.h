#pragma once

// Custom Classes
#include "CDTable.h"

struct CDScriptComponent {
	unsigned int id;                        //!< The component ID
	std::string script_name;           //!< The script name
	std::string client_script_name;    //!< The client script name
};

class CDScriptComponentTable : public CDTable {
private:
	std::map<unsigned int, CDScriptComponent> entries;
	CDScriptComponent m_ToReturnWhenNoneFound;

public:
	CDScriptComponentTable();

	static const std::string GetTableName() { return "ScriptComponent"; };

	// Gets an entry by scriptID
	const CDScriptComponent& GetByID(unsigned int id);
};

