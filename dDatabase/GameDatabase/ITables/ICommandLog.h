#ifndef __ICOMMANDLOG__H__
#define __ICOMMANDLOG__H__

#include <cstdint>
#include <string>
#include <string_view>

class ICommandLog {
public:

	// Insert a new slash command log entry.
	virtual void InsertSlashCommandUsage(const LWOOBJID characterId, const std::string_view command) = 0;

	// Get paginated command log data for DataTables display
	virtual std::string GetCommandLogTable(uint32_t start, uint32_t length, const std::string_view search = "", uint32_t orderColumn = 0, bool orderAsc = true) = 0;
};

#endif  //!__ICOMMANDLOG__H__
