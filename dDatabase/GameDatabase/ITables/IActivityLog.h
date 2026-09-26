#ifndef __IACTIVITYLOG__H__
#define __IACTIVITYLOG__H__

#include <cstdint>
#include <string>
#include <string_view>

#include "dCommonVars.h"

enum class eActivityType : uint32_t {
	PlayerLoggedIn,
	PlayerLoggedOut,
	PlayerChangedZone
};

class IActivityLog {
public:
	// Update the activity log for the given account.
	virtual void UpdateActivityLog(const LWOOBJID characterId, const eActivityType activityType, const LWOMAPID mapId) = 0;

	// Get paginated activity log data for DataTables display
	virtual std::string GetActivityLogTable(uint32_t start, uint32_t length, const std::string_view search = "", uint32_t orderColumn = 0, bool orderAsc = true) = 0;

	// Get total count of activity log entries
	virtual uint32_t GetActivityLogCount() = 0;
};

#endif  //!__IACTIVITYLOG__H__
