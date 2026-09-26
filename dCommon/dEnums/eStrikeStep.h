#ifndef __ESTRIKESTEP__H__
#define __ESTRIKESTEP__H__

#include <cstdint>

// What happens on its own when an account reaches a number of active strikes (strike_*_at settings).
// account_strikes.step holds the name on the strike that set it off.
enum class eStrikeStep : uint8_t {
	WARN, // a warning on the account's record, shown to the player if they're online
	MUTE, // muted for strike_mute_days
	BAN,  // banned for strike_ban_days (0: permanently)
};

#endif  //!__ESTRIKESTEP__H__
