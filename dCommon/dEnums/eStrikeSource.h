#ifndef __ESTRIKESOURCE__H__
#define __ESTRIKESOURCE__H__

#include <cstdint>

// What a strike against an account was given for (account_strikes.source holds the name)
enum class eStrikeSource : uint8_t {
	MANUAL,      // given by hand on the account's page
	NAME,        // a rejected character name
	PET_NAME,    // a rejected pet name
	PROPERTY,    // a rejected property
	LEADERBOARD, // a score removed from a leaderboard
	PLAYER_REPORT, // acting on a report a player sent from the game (Player Reports)
};

#endif  //!__ESTRIKESOURCE__H__
