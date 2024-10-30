#ifndef EQUICKBUILDSTATE_H
#define EQUICKBUILDSTATE_H

#include <cstdint>

enum class eQuickBuildState : uint32_t {
	OPEN,
	COMPLETED = 2,
	RESETTING = 4,
	BUILDING,
	INCOMPLETE
};


#endif  //!EQUICKBUILDSTATE_H
