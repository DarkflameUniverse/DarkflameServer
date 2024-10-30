#ifndef EPETABILITYTYPE_H
#define EPETABILITYTYPE_H

#include <cstdint>

enum class ePetAbilityType : uint32_t {
	Invalid,
	GoToObject,
	JumpOnObject,
	DigAtPosition
};

#endif  //!EPETABILITYTYPE_H
