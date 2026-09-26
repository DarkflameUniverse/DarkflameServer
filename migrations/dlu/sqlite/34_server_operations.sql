/* Scheduled announcements, events, zone limits and per-instance player samples. See the MySQL migration. */
CREATE TABLE IF NOT EXISTS scheduled_announcements (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    title TEXT NOT NULL DEFAULT '',
    message TEXT NOT NULL,
    zones TEXT NOT NULL DEFAULT '',
    schedule TEXT NOT NULL,
    starts_at BIGINT NOT NULL DEFAULT 0,
    ends_at BIGINT NOT NULL DEFAULT 0,
    enabled INTEGER NOT NULL DEFAULT 1,
    last_sent_at BIGINT NOT NULL DEFAULT 0,
    sent_count INTEGER NOT NULL DEFAULT 0,
    created_at BIGINT NOT NULL DEFAULT 0,
    created_by TEXT NOT NULL DEFAULT '',
    updated_at BIGINT NOT NULL DEFAULT 0,
    updated_by TEXT NOT NULL DEFAULT ''
);
CREATE TABLE IF NOT EXISTS scheduled_events (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    feature TEXT NOT NULL,
    note TEXT NOT NULL DEFAULT '',
    starts_at BIGINT NOT NULL,
    ends_at BIGINT NOT NULL,
    state INTEGER NOT NULL DEFAULT 0,
    slot INTEGER NOT NULL DEFAULT 0,
    previous_value TEXT NULL DEFAULT NULL,
    previous_web_wins INTEGER NOT NULL DEFAULT 0,
    status TEXT NOT NULL DEFAULT '',
    created_at BIGINT NOT NULL DEFAULT 0,
    created_by TEXT NOT NULL DEFAULT '',
    updated_at BIGINT NOT NULL DEFAULT 0,
    updated_by TEXT NOT NULL DEFAULT ''
);
CREATE TABLE IF NOT EXISTS zone_limits (
    zone_id INTEGER NOT NULL PRIMARY KEY,
    soft_cap INTEGER NULL DEFAULT NULL,
    hard_cap INTEGER NULL DEFAULT NULL,
    spare_instances INTEGER NOT NULL DEFAULT 0,
    updated_at BIGINT NOT NULL DEFAULT 0,
    updated_by TEXT NOT NULL DEFAULT ''
);
CREATE TABLE IF NOT EXISTS server_health_instances (
    time BIGINT NOT NULL,
    zone_id INTEGER NOT NULL,
    instance_id INTEGER NOT NULL,
    clone_id INTEGER NOT NULL DEFAULT 0,
    players INTEGER NOT NULL DEFAULT 0,
    PRIMARY KEY (time, zone_id, instance_id)
);
CREATE INDEX IF NOT EXISTS server_health_instances_zone ON server_health_instances (zone_id, time);
