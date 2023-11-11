#ifndef __IACTIVITYLOG__H__
#define __IACTIVITYLOG__H__

#include <cstdint>

#include "dCommonVars.h"

enum class eActivityType : uint32_t {
	PlayerLoggedIn,
	PlayerLoggedOut,
};

class IActivityLog {
public:
	// Update the activity log for the given account.
	virtual void UpdateActivityLog(const uint32_t characterId, const eActivityType activityType, const LWOMAPID mapId) = 0;
};

#endif  //!__IACTIVITYLOG__H__
