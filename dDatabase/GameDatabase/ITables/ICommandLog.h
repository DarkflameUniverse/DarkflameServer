#ifndef __ICOMMANDLOG__H__
#define __ICOMMANDLOG__H__

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

class ICommandLog {
public:
	struct Entry {
		uint64_t timestamp{};
		LWOOBJID characterId{};
		std::string command;
		std::string arguments;
	};

	// Insert a new slash command log entry.
	virtual void InsertSlashCommandUsage(const LWOOBJID characterId, const std::string_view command) = 0;

	// Get recent command log entries
	virtual std::vector<Entry> GetCommandLogs(uint32_t limit = 100) = 0;
};

#endif  //!__ICOMMANDLOG__H__
