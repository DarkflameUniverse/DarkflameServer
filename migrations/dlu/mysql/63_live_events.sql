/* Live events staff start from the dashboard (treasure hunts, bonus multipliers, invasions, celebrations). type names
   the kind, config is its JSON settings. zones: comma separated zone IDs ('' for every world, bonus events only);
   instance_id -1 for every instance of them. state: 0 running, 1 ended (time up or ended early), 2 cancelled. World
   servers run each event in every matching instance and clean up after themselves at ends_at or when it is ended. */
CREATE TABLE IF NOT EXISTS live_events (
    id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
    type VARCHAR(32) NOT NULL,
    title VARCHAR(100) NOT NULL DEFAULT '',
    message TEXT NOT NULL,
    zones TEXT NOT NULL,
    instance_id INT NOT NULL DEFAULT -1,
    config TEXT NOT NULL,
    starts_at BIGINT NOT NULL,
    ends_at BIGINT NOT NULL,
    state TINYINT NOT NULL DEFAULT 0,
    ended_at BIGINT NOT NULL DEFAULT 0,
    end_reason VARCHAR(255) NOT NULL DEFAULT '',
    created_by VARCHAR(64) NOT NULL DEFAULT '',
    ended_by VARCHAR(64) NOT NULL DEFAULT '',
    INDEX live_events_state (state)
);
/* What each world instance running an event last reported (JSON: spawned, found, kills, wave, ...). closed = 1 once
   the instance stopped running it (event over or world shut down). */
CREATE TABLE IF NOT EXISTS live_event_instances (
    event_id BIGINT UNSIGNED NOT NULL,
    zone_id INT UNSIGNED NOT NULL,
    instance_id INT UNSIGNED NOT NULL,
    status TEXT NOT NULL,
    updated_at BIGINT NOT NULL DEFAULT 0,
    PRIMARY KEY (event_id, zone_id, instance_id)
);
/* Per character score in an event: treasures found, invaders smashed. */
CREATE TABLE IF NOT EXISTS live_event_scores (
    event_id BIGINT UNSIGNED NOT NULL,
    character_id BIGINT NOT NULL,
    amount BIGINT NOT NULL DEFAULT 0,
    updated_at BIGINT NOT NULL DEFAULT 0,
    PRIMARY KEY (event_id, character_id)
);
