/* Daily totals of player statistics. See the MySQL migration. */
CREATE TABLE IF NOT EXISTS player_stats_daily (
    day INTEGER NOT NULL,
    zone INTEGER NOT NULL,
    stat INTEGER NOT NULL,
    gm INTEGER NOT NULL DEFAULT 0,
    amount BIGINT NOT NULL DEFAULT 0,
    PRIMARY KEY (day, zone, stat, gm)
);
