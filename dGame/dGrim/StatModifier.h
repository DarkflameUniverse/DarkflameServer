#pragma once

#ifndef __ESTATMODIFIER__H__
#define __ESTATMODIFIER__H__

#include <cstdint>

enum class eStatModifier : uint32_t {
	Absolute = 0,
	Percent = 1,
	DamageAbsolute = 2,
	DamagePercent = 3,
	DamageResistance = 4,

	MAX
};

#endif  //!__ESTATMODIFIER__H__