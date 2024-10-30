#ifndef ICOMMANDLOG_H
#define ICOMMANDLOG_H

#include <cstdint>
#include <string_view>

class ICommandLog {
public:	

	// Insert a new slash command log entry.
	virtual void InsertSlashCommandUsage(const uint32_t characterId, const std::string_view command) = 0;
};

#endif  //!ICOMMANDLOG_H
