/* Player position samples for replays. See the MySQL migration. */
CREATE TABLE IF NOT EXISTS player_positions (
    character_id BIGINT NOT NULL,
    time BIGINT NOT NULL,
    zone_id INTEGER NOT NULL,
    instance_id INTEGER NOT NULL,
    clone_id INTEGER NOT NULL DEFAULT 0,
    x REAL NOT NULL,
    y REAL NOT NULL,
    z REAL NOT NULL,
    PRIMARY KEY (character_id, time)
);
CREATE INDEX IF NOT EXISTS player_positions_zone_time ON player_positions (zone_id, time);
CREATE INDEX IF NOT EXISTS player_positions_time ON player_positions (time);
