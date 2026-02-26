#ifndef __IBUGREPORTS__H__
#define __IBUGREPORTS__H__

#include <cstdint>
#include <string>
#include <string_view>

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
	// Returns a JSON-formatted string with the bug report data and metadata
	virtual std::string GetBugReportsTable(uint32_t start, uint32_t length, const std::string_view search = "", uint32_t orderColumn = 0, bool orderAsc = true) = 0;
};
#endif  //!__IBUGREPORTS__H__
