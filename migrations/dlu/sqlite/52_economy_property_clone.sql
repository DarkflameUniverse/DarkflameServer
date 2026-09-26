/* map_events_daily and player_stats_daily get clone_id in their keys: see the MySQL migration. SQLite can't change a
   primary key, so the tables are rebuilt. */
BEGIN TRANSACTION;
CREATE TABLE map_events_daily_2 (
    day INTEGER NOT NULL,
    zone INTEGER NOT NULL,
    kind INTEGER NOT NULL,
    lot INTEGER NOT NULL,
    cell_x INTEGER NOT NULL,
    cell_z INTEGER NOT NULL,
    events BIGINT NOT NULL DEFAULT 0,
    quantity BIGINT NOT NULL DEFAULT 0,
    clone_id INTEGER NOT NULL DEFAULT 0,
    PRIMARY KEY (zone, kind, day, clone_id, lot, cell_x, cell_z)
);
INSERT INTO map_events_daily_2 (day, zone, kind, lot, cell_x, cell_z, events, quantity)
    SELECT day, zone, kind, lot, cell_x, cell_z, events, quantity FROM map_events_daily;
DROP TABLE map_events_daily;
ALTER TABLE map_events_daily_2 RENAME TO map_events_daily;
CREATE TABLE player_stats_daily_2 (
    day INTEGER NOT NULL,
    zone INTEGER NOT NULL,
    stat INTEGER NOT NULL,
    gm INTEGER NOT NULL DEFAULT 0,
    amount BIGINT NOT NULL DEFAULT 0,
    clone_id INTEGER NOT NULL DEFAULT 0,
    PRIMARY KEY (day, zone, clone_id, stat, gm)
);
INSERT INTO player_stats_daily_2 (day, zone, stat, gm, amount)
    SELECT day, zone, stat, gm, amount FROM player_stats_daily;
DROP TABLE player_stats_daily;
ALTER TABLE player_stats_daily_2 RENAME TO player_stats_daily;
COMMIT;
