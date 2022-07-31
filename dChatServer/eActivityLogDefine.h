#pragma once
#ifndef __EACTIVITYLOGDEFINE__H__
#define __EACTIVITYLOGDEFINE__H__

#include <cstdint>

enum class eActivityLogDefine : uint8_t {
	PLAYER_ENTERED_WORLD = 0,
	PLAYER_LEFT_WORLD
};

#endif  //!__EACTIVITYLOGDEFINE__H__
