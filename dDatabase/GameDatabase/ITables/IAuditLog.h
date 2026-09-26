#ifndef __IAUDITLOG__H__
#define __IAUDITLOG__H__

#include <cstdint>
#include <string>
#include <string_view>

#include "dCommonVars.h"

class IAuditLog {
public:
	// targetAccountId / targetCharacterId: what the action was about (0: nothing in particular)
	virtual void InsertAuditLog(uint32_t accountId, const std::string_view accountName, const std::string_view action, const std::string_view description,
		uint32_t targetAccountId, LWOOBJID targetCharacterId) = 0;
	virtual std::string GetAuditLogTable(uint32_t start, uint32_t length, const std::string_view search = "", uint32_t orderColumn = 0, bool orderAsc = true) = 0;
};

#endif  //!__IAUDITLOG__H__
