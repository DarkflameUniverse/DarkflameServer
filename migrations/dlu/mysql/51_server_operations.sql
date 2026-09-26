/* Repeating in-game announcements scheduled on the dashboard. zones: comma separated zone IDs, '' for every world.
   schedule: a cron expression or @every interval in UTC, as for scheduled tasks. starts_at/ends_at 0: no limit. */
CREATE TABLE IF NOT EXISTS scheduled_announcements (
    id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
    title VARCHAR(100) NOT NULL DEFAULT '',
    message TEXT NOT NULL,
    zones TEXT NOT NULL,
    schedule VARCHAR(128) NOT NULL,
    starts_at BIGINT NOT NULL DEFAULT 0,
    ends_at BIGINT NOT NULL DEFAULT 0,
    enabled TINYINT NOT NULL DEFAULT 1,
    last_sent_at BIGINT NOT NULL DEFAULT 0,
    sent_count INT UNSIGNED NOT NULL DEFAULT 0,
    created_at BIGINT NOT NULL DEFAULT 0,
    created_by VARCHAR(64) NOT NULL DEFAULT '',
    updated_at BIGINT NOT NULL DEFAULT 0,
    updated_by VARCHAR(64) NOT NULL DEFAULT ''
);
/* Events: a feature name (gatingOnFeature in the levels, FeatureGating in the client database) put in a free
   event_N setting from starts_at to ends_at. state: 0 scheduled, 1 active, 2 ended, 3 cancelled, 4 missed (the
   dashboard was down for all of it). slot: the N it used. previous_value/previous_web_wins: the slot's
   sharedconfig.ini web value before, put back at the end. */
CREATE TABLE IF NOT EXISTS scheduled_events (
    id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
    feature VARCHAR(128) NOT NULL,
    note TEXT NOT NULL,
    starts_at BIGINT NOT NULL,
    ends_at BIGINT NOT NULL,
    state TINYINT NOT NULL DEFAULT 0,
    slot TINYINT NOT NULL DEFAULT 0,
    previous_value TEXT NULL DEFAULT NULL,
    previous_web_wins TINYINT NOT NULL DEFAULT 0,
    status TEXT NOT NULL,
    created_at BIGINT NOT NULL DEFAULT 0,
    created_by VARCHAR(64) NOT NULL DEFAULT '',
    updated_at BIGINT NOT NULL DEFAULT 0,
    updated_by VARCHAR(64) NOT NULL DEFAULT ''
);
/* Per-zone player caps and spare instances the master server applies to public instances of the zone. A NULL cap
   uses the client's ZoneTable population_soft_cap / population_hard_cap. */
CREATE TABLE IF NOT EXISTS zone_limits (
    zone_id INT UNSIGNED NOT NULL PRIMARY KEY,
    soft_cap INT UNSIGNED NULL DEFAULT NULL,
    hard_cap INT UNSIGNED NULL DEFAULT NULL,
    spare_instances INT UNSIGNED NOT NULL DEFAULT 0,
    updated_at BIGINT NOT NULL DEFAULT 0,
    updated_by VARCHAR(64) NOT NULL DEFAULT ''
);
/* Players in each world instance, sampled with server_health once a minute and kept as long (health_days). */
CREATE TABLE IF NOT EXISTS server_health_instances (
    time BIGINT NOT NULL,
    zone_id INT UNSIGNED NOT NULL,
    instance_id INT UNSIGNED NOT NULL,
    clone_id INT UNSIGNED NOT NULL DEFAULT 0,
    players INT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (time, zone_id, instance_id),
    INDEX server_health_instances_zone (zone_id, time)
);
