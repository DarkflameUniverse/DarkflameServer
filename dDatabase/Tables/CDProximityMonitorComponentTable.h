#pragma once

// Custom Classes
#include "CDTable.h"

struct CDProximityMonitorComponent {
	unsigned int id;
	std::string Proximities;
	bool LoadOnClient;
	bool LoadOnServer;
};

class CDProximityMonitorComponentTable : public CDTable<CDProximityMonitorComponentTable> {
private:
	std::vector<CDProximityMonitorComponent> entries;

public:
	CDProximityMonitorComponentTable(void);
	//! Queries the table with a custom "where" clause
	std::vector<CDProximityMonitorComponent> Query(std::function<bool(CDProximityMonitorComponent)> predicate);

	std::vector<CDProximityMonitorComponent> GetEntries(void) const;
};
