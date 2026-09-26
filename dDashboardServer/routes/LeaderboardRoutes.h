#pragma once

#include <cstddef>

#include "json.hpp"

/**
 * Leaderboards for everyone signed in (the same ranking the game shows, with your own characters picked out), and
 * leaderboard moderation for staff: remove one character's score or clear a whole board.
 */
void RegisterLeaderboardRoutes();

/**
 * The first `perBoard` places of every leaderboard that has scores, for the public status page:
 * [{id, name, columns, top: [{rank, name, primary, secondary, tertiary, wins, played}]}]. Character names only; no IDs.
 */
nlohmann::json LeaderboardTops(size_t perBoard);
