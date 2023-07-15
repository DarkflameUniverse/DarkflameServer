#pragma once

#ifndef __ESTATRARITY__H__
#define __ESTATRARITY__H__

#include <cstdint>

enum class eStatRarity : uint32_t {
	Common = 0,
	Uncommon = 1,
	Rare = 2,
	Epic = 3,
	Legendary = 4,
	Relic = 5,

	MAX
};

#endif  //!__ESTATRARITY__H__