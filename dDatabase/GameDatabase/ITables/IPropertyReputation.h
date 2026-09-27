#ifndef __IPROPERTYREPUTATION__H__
#define __IPROPERTYREPUTATION__H__

#include <cstdint>

#include "dCommonVars.h"
#include "json.hpp"

/**
 * Property reputation (see dCommon/PropertyReputationRules.h): what visitors gave each property per day.
 */
class IPropertyReputation {
public:
	struct VisitorHistory {
		int64_t today{};          // points the account gave the property today
		uint32_t previousDays{};  // other days in the window it gave the property any
	};

	// days: how many days before `day` count as recent
	virtual VisitorHistory GetPropertyVisitorHistory(LWOOBJID propertyId, uint32_t accountId, uint32_t day, uint32_t days) = 0;

	// Points the property got from everyone on a day
	virtual int64_t GetPropertyReputationOnDay(LWOOBJID propertyId, uint32_t day) = 0;

	// Add a visitor's points and time for a day, and the points to properties.reputation
	virtual void AddPropertyReputation(LWOOBJID propertyId, uint32_t accountId, uint32_t day, int64_t points, int64_t seconds) = 0;

	// For the property page: [{day, visitors, points, seconds}] of the last `days` days, newest first
	virtual nlohmann::json GetPropertyReputationDays(LWOOBJID propertyId, uint32_t fromDay) = 0;
};

#endif  //!__IPROPERTYREPUTATION__H__
