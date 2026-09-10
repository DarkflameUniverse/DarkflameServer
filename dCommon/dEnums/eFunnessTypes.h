#ifndef EFUNNESSTYPES_H
#define EFUNNESSTYPES_H

#include <cstdint>

// Most of these are guesses from the client, some are unused (maybe server side only?) and
// some are really hard to pin a definition down for because they are buried in havok stuff
enum class eFunnessTypes : int32_t {
	DebuggerActive = 1,
	CharacterPosLength = 2,
	CharacterVelLength = 5,
	CharacterRunMultiplier = 6,
	CharacterGravityScale = 7,
	Unknown_9 = 9,
	Unknown_10 = 10,
	RacingBoostTimeTooLong = 12,
	SomeRacingManipCheat = 13,
	FdbFailedChecksum = 14,
};

struct CaughtFunness {
	eFunnessTypes cheatType{};
	float cheatInfo{};
};

#endif  //!EFUNNESSTYPES_H
