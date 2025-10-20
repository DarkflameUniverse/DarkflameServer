#ifndef IBEHAVIORS_H
#define IBEHAVIORS_H

#include <cstdint>

#include "dCommonVars.h"

class IBehaviors {
public:
	struct Info {
		LWOOBJID behaviorId{};
		LWOOBJID characterId{};
		std::string behaviorInfo;
	};

	// This Add also takes care of updating if it exists.
	virtual void AddBehavior(const Info& info) = 0;
	virtual std::string GetBehavior(const LWOOBJID behaviorId) = 0;
	virtual void RemoveBehavior(const LWOOBJID behaviorId) = 0;
};

#endif  //!IBEHAVIORS_H
