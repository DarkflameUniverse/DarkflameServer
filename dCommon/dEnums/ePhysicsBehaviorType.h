#ifndef __EPHYSICSBEHAVIORTYPE__H__
#define __EPHYSICSBEHAVIORTYPE__H__

#include <cstdint>

enum class ePhysicsBehaviorType : int32_t {
	INVALID = -1,
	GROUND,
	FLYING,
	STANDARD,
	DYNAMIC
};

#endif  //!__EPHYSICSBEHAVIORTYPE__H__
