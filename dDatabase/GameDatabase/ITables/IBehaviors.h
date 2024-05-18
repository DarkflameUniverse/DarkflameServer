#ifndef __IBEHAVIORS__H__
#define __IBEHAVIORS__H__

#include <cstdint>

#include "dCommonVars.h"

class IBehaviors {
public:
	struct Info {
		LWOOBJID behaviorId{};
		uint32_t characterId{};
		std::string behaviorInfo;
	};

	virtual void AddBehavior(const Info& info) = 0;
	virtual void RemoveBehavior(const uint32_t behaviorId) = 0;
};

#endif  //!__IBEHAVIORS__H__
