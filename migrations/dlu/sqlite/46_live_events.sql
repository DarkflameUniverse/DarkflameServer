/* Live events started from the dashboard. See the MySQL migration. */
CREATE TABLE IF NOT EXISTS live_events (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    type TEXT NOT NULL,
    title TEXT NOT NULL DEFAULT '',
    message TEXT NOT NULL DEFAULT '',
    zones TEXT NOT NULL DEFAULT '',
    instance_id INTEGER NOT NULL DEFAULT -1,
    config TEXT NOT NULL DEFAULT '{}',
    starts_at BIGINT NOT NULL,
    ends_at BIGINT NOT NULL,
    state INTEGER NOT NULL DEFAULT 0,
    ended_at BIGINT NOT NULL DEFAULT 0,
    end_reason TEXT NOT NULL DEFAULT '',
    created_by TEXT NOT NULL DEFAULT '',
    ended_by TEXT NOT NULL DEFAULT ''
);
CREATE INDEX IF NOT EXISTS live_events_state ON live_events (state);
CREATE TABLE IF NOT EXISTS live_event_instances (
    event_id INTEGER NOT NULL,
    zone_id INTEGER NOT NULL,
    instance_id INTEGER NOT NULL,
    status TEXT NOT NULL DEFAULT '{}',
    updated_at BIGINT NOT NULL DEFAULT 0,
    PRIMARY KEY (event_id, zone_id, instance_id)
);
CREATE TABLE IF NOT EXISTS live_event_scores (
    event_id INTEGER NOT NULL,
    character_id BIGINT NOT NULL,
    amount BIGINT NOT NULL DEFAULT 0,
    updated_at BIGINT NOT NULL DEFAULT 0,
    PRIMARY KEY (event_id, character_id)
);
