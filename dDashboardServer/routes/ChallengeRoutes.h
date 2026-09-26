#pragma once

#include "json.hpp"

/**
 * Server-wide community challenges (Challenges page): reach a target of a player statistic (StatisticID) or map event
 * (IEconomyLedger::eMapEvent) together between two times. World servers count each character's part where the game
 * records it (LiveEvents.h in dGame); the dashboard adds them up, announces milestones in game, and when the target is
 * reached mails the rewards to every character that contributed enough (coins wait for them in game: /challenge).
 */
namespace ChallengeRoutes {
	void RegisterRoutes();

	// Main loop: milestones, completion and rewards, expiry
	void Update();

	// Public challenges for the status page: running ones and those finished in the last week
	nlohmann::json PublicJson();
}
