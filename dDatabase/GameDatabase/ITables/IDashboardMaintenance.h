#ifndef __IDASHBOARDMAINTENANCE__H__
#define __IDASHBOARDMAINTENANCE__H__

#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "dCommonVars.h"

// Bulk data reads and repairs for dashboard reports and scheduled tasks
class IDashboardMaintenance {
public:
	// Approve pending pet names that an earlier pet with the same name was approved for. Returns rows changed.
	virtual uint32_t ApprovePreviouslyApprovedPetNames() = 0;

	// Call visit for every character's XML, one row at a time
	virtual void ForEachCharacterXml(const std::function<void(LWOOBJID, const std::string&)>& visit) = 0;

	// Same, but only characters whose XML contains `needle`; the database does the filtering
	virtual void ForEachCharacterXmlContaining(const std::string& needle, const std::function<void(LWOOBJID, const std::string&)>& visit) = 0;
};

#endif  //!__IDASHBOARDMAINTENANCE__H__
