#ifndef __EMOVEMENTPLATFORMSTATE__H__
#define __EMOVEMENTPLATFORMSTATE__H__

#include <cstdint>

/**
 * The different types of platform movement state, supposedly a bitmap
 */
enum class eMovementPlatformState : uint32_t
{
	Moving = 0b00010,
	Stationary = 0b11001,
	Stopped = 0b01100
};

#endif  //!__EMOVEMENTPLATFORMSTATE__H__
