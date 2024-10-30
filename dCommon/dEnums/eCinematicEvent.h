#ifndef ECINEMATICEVENT_H
#define ECINEMATICEVENT_H

#include <cstdint>

enum class eCinematicEvent : uint32_t {
	STARTED,
	WAYPOINT,
	ENDED,
};

#endif  //!ECINEMATICEVENT_H
