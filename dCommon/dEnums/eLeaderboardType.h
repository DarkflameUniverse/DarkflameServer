#ifndef __ELEADERBOARDTYPE__H__
#define __ELEADERBOARDTYPE__H__

#include <cstdint>

/**
 * CDClient Activities.leaderboardType. The client picks the leaderboard's columns from it
 * (LWOCharacterComponent::msgSendActivitySummaryLeaderboardData in 1.10.64); the server decides the order.
 */
enum class eLeaderboardType : uint32_t {
	ShootingGallery,    // Score, Streak, Accuracy
	Racing,             // Best Time, Best Lap, Wins
	MonumentRace,       // Time taken
	FootRace,           // Time left on the race's countdown at the finish (L_ACT_BASE_FOOT_RACE_CLIENT.lua sends the remaining time)
	UnusedLeaderboard4, // There is no 4 defined anywhere in the cdclient, but it takes Points.
	Survival,           // Time, Points
	SurvivalNS,         // Wave, Time
	Donations,          // Donations
	None
};

#endif  //!__ELEADERBOARDTYPE__H__
