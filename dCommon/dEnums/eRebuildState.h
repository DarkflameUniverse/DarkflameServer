#ifndef __EREBUILDSTATE__H__
#define __EREBUILDSTATE__H__

#include <cstdint>

enum class eRebuildState : uint32_t {
	OPEN,
	COMPLETED = 2,
	RESETTING = 4,
	BUILDING,
	INCOMPLETE
};


#endif  //!__EREBUILDSTATE__H__
