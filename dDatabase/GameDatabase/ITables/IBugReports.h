#ifndef __IBUGREPORTS__H__
#define __IBUGREPORTS__H__

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <optional>

class IBugReports {
public:
	struct Info {
		std::string body;
		std::string clientVersion;
		std::string otherPlayer;
		std::string selection;
		LWOOBJID characterId{};
	};

	struct DetailedInfo {
		uint64_t id{};
		std::string body;
		std::string clientVersion;
		std::string otherPlayer;
		std::string selection;
		LWOOBJID characterId{};
		uint64_t submitted{};
		uint64_t resolved_time{};
		uint32_t resolved_by_id{};
		std::string resolution;
	};

	// Add a new bug report to the database.
	virtual void InsertNewBugReport(const Info& info) = 0;

	// Dashboard methods
	virtual std::vector<DetailedInfo> GetAllBugReports() = 0;
	virtual std::vector<DetailedInfo> GetUnresolvedBugReports() = 0;
	virtual std::vector<DetailedInfo> GetResolvedBugReports() = 0;
	virtual std::optional<DetailedInfo> GetBugReportById(const uint64_t reportId) = 0;
	virtual void ResolveBugReport(const uint64_t reportId, const uint32_t resolvedById, const std::string_view resolution) = 0;
	virtual uint32_t GetBugReportCount() = 0;
	virtual uint32_t GetUnresolvedBugReportCount() = 0;
};
#endif  //!__IBUGREPORTS__H__
