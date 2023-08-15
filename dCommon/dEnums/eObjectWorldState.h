#ifndef __EOBJECTWORLDSTATE__H__
#define __EOBJECTWORLDSTATE__H__

#include <cstdint>

enum class eObjectWorldState : uint32_t {
	INWORLD,
	ATTACHED,
	INVENTORY
};

#endif  //!__EOBJECTWORLDSTATE__H__
