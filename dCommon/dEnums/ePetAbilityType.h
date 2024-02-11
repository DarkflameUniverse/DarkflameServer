#ifndef __EPETABILITYTYPE__H__
#define __EPETABILITYTYPE__H__

#include <cstdint>

enum class ePetAbilityType : uint32_t {
	Invalid,
	GoToObject,
	JumpOnObject,
	DigAtPosition
};

#endif  //!__EPETABILITYTYPE__H__
