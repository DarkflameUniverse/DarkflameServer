#ifndef EGAMEACTIVITY_H
#define EGAMEACTIVITY_H

#include <cstdint>

enum class eGameActivity : uint32_t {
	NONE,
	QUICKBUILDING,
	SHOOTING_GALLERY,
	RACING,
	PINBALL,
	PET_TAMING
};

#endif  //!EGAMEACTIVITY_H
