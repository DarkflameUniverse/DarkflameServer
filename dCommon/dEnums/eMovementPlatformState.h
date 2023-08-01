#ifndef __EMOVEMENTPLATFORMSTATE__H__
#define __EMOVEMENTPLATFORMSTATE__H__

#include <cstdint>

/**
 * The different types of platform movement state
 */
enum class eMovementPlatformState : uint32_t
{
	Waiting = 1 << 0U,
	Travelling = 1 << 1U,
	Stopped = 1 << 2U,
	ReachedDesiredWaypoint = 1 << 3U,
	ReachedFinalWaypoint = 1 << 4U,
};

inline constexpr eMovementPlatformState operator|(eMovementPlatformState a, eMovementPlatformState b) {
	return static_cast<eMovementPlatformState>(
		static_cast<std::underlying_type<eMovementPlatformState>::type>(a) |
		static_cast<std::underlying_type<eMovementPlatformState>::type>(b));
};

#endif  //!__EMOVEMENTPLATFORMSTATE__H__
