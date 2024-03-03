#pragma once

// Custom Classes
#include "CDTable.h"

struct CDProximityMonitorComponent {
	uint32_t id;
	std::string Proximities;
	bool LoadOnClient;
	bool LoadOnServer;
};

class CDProximityMonitorComponentTable : public CDTable<CDProximityMonitorComponentTable, std::vector<CDProximityMonitorComponent>> {
public:
	void LoadValuesFromDatabase();
	//! Queries the table with a custom "where" clause
	std::vector<CDProximityMonitorComponent> Query(std::function<bool(CDProximityMonitorComponent)> predicate);
};
