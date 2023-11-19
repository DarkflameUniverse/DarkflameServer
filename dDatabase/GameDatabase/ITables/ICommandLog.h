#ifndef __ICOMMANDLOG__H__
#define __ICOMMANDLOG__H__

#include <cstdint>
#include <string_view>

class ICommandLog {
public:	

	// Insert a new slash command log entry.
	virtual void InsertSlashCommandUsage(const uint32_t characterId, const std::string_view command) = 0;
};

#endif  //!__ICOMMANDLOG__H__
