#ifndef EOBJECTWORLDSTATE_H
#define EOBJECTWORLDSTATE_H

#include <cstdint>

enum class eObjectWorldState : uint32_t {
	INWORLD,
	ATTACHED,
	INVENTORY
};

#endif  //!EOBJECTWORLDSTATE_H
