#ifndef __IACTIVITYLOG__H__
#define __IACTIVITYLOG__H__

#include <cstdint>

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

	struct Entry {
		LWOOBJID characterId{};
		eActivityType activity{};
		uint32_t timestamp{};
		LWOMAPID mapId{};
	};

	// Retrieve recent activity entries ordered by time desc.
	virtual std::vector<Entry> GetRecentActivity(const uint32_t limit) = 0;

	// Get total count of activity log entries
	virtual uint32_t GetActivityLogCount() = 0;

	// Get paginated activity log entries with ordering
	virtual std::vector<Entry> GetActivityLogPaginated(
		uint32_t offset,
		uint32_t limit,
		const std::string& orderColumn = "time",
		const std::string& orderDir = "DESC"
	) = 0;
};

#endif  //!__IACTIVITYLOG__H__
