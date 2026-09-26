#ifndef __IBUGREPORTS__H__
#define __IBUGREPORTS__H__

#include <cstdint>
#include <string>
#include <string_view>

#include "dCommonVars.h"
#include "json.hpp"

class IBugReports {
public:
	struct Info {
		std::string body;
		std::string clientVersion;
		std::string otherPlayer;
		std::string selection;
		LWOOBJID characterId{};
	};

	// Add a new bug report to the database.
	virtual void InsertNewBugReport(const Info& info) = 0;

	// Get paginated list of bug reports with optional search/filtering for DataTables
	// resolvedFilter: -1 = all, 0 = unresolved only, 1 = resolved only
	virtual std::string GetBugReportsTable(uint32_t start, uint32_t length, const std::string_view search = "", uint32_t orderColumn = 0, bool orderAsc = true, int8_t resolvedFilter = -1) = 0;

	// Get a single bug report with reporter and resolution details, or {"error": ...} if missing
	virtual nlohmann::json GetBugReport(const uint32_t id) = 0;

	// Mark a bug report resolved. Column names match NexusDashboard so existing databases keep their data.
	virtual void ResolveBugReport(const uint32_t id, const uint32_t resolverAccountId, const std::string_view resolution) = 0;

	virtual void DeleteBugReport(const uint32_t id) = 0;

	virtual uint32_t GetBugReportCount() = 0;
};
#endif  //!__IBUGREPORTS__H__
