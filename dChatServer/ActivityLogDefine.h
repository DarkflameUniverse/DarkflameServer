#pragma once
#ifndef __ACTIVITYLOGDEFINE__H__
#define __ACTIVITYLOGDEFINE__H__

#include <cstdint>

enum class ActivityLogDefine : uint8_t {
	PLAYER_ENTERED_WORLD = 0,
	PLAYER_LEFT_WORLD
};

#endif  //!__ACTIVITYLOGDEFINE__H__
