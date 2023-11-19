#ifndef __IBUGREPORTS__H__
#define __IBUGREPORTS__H__

#include <cstdint>
#include <string_view>

class IBugReports {
public:
	struct Info {
		std::string body;
		std::string clientVersion;
		std::string otherPlayer;
		std::string selection;
		uint32_t characterId{};
	};

	// Add a new bug report to the database.
	virtual void InsertNewBugReport(const Info& info) = 0;
};
#endif  //!__IBUGREPORTS__H__
