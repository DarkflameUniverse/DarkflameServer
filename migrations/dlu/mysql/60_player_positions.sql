/* Where players were, sampled every few seconds by the dashboard for the 3D world view's replays (staff only). Kept for position_history_days. */
CREATE TABLE IF NOT EXISTS player_positions (
    character_id BIGINT NOT NULL,
    time BIGINT NOT NULL,
    zone_id INT UNSIGNED NOT NULL,
    instance_id INT UNSIGNED NOT NULL,
    clone_id INT UNSIGNED NOT NULL DEFAULT 0,
    x FLOAT NOT NULL,
    y FLOAT NOT NULL,
    z FLOAT NOT NULL,
    PRIMARY KEY (character_id, time),
    INDEX player_positions_zone_time (zone_id, time),
    INDEX player_positions_time (time)
);
