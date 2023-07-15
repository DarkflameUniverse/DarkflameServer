#pragma once

#ifndef __ESTATTYPES__H__
#define __ESTATTYPES__H__

#include <cstdint>

enum class eStatTypes : uint32_t {
	// Stats
	Health = 0,
	Armor = 1,
	Imagination = 2,

	// Damage
	Physical = 3,
	Electric = 4,
	Corruption = 5,
	Heat = 6,
	Shadow = 7,
	Pierce = 8,
	Vitality = 9,
	Domination = 10,

	MAX
};

#endif  //!__ESTATTYPES__H__