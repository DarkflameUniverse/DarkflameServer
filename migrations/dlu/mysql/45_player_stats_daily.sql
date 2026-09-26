/* Daily totals of the per-character statistics (StatisticID: smashables smashed, quickbuilds, missions, pets tamed,
   times smashed, ...) summed over every player in a zone. gm = 1 for staff (GM 3+), so reports can leave them out.
   The map event kinds in map_events_daily grow too: 4 = player death (lot = killer), 5 = coins a player dropped on
   death, 6 = smashable smashed by a player, 7 = quickbuild completed. */
CREATE TABLE IF NOT EXISTS player_stats_daily (
    day INT NOT NULL,
    zone INT NOT NULL,
    stat INT NOT NULL,
    gm TINYINT NOT NULL DEFAULT 0,
    amount BIGINT NOT NULL DEFAULT 0,
    PRIMARY KEY (day, zone, stat, gm)
);
