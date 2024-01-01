#ifndef __EQUICKBUILDSTATE__H__
#define __EQUICKBUILDSTATE__H__

#include <cstdint>

enum class eQuickBuildState : uint32_t {
	OPEN,
	COMPLETED = 2,
	RESETTING = 4,
	BUILDING,
	INCOMPLETE
};


#endif  //!__EQUICKBUILDSTATE__H__
