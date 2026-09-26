/* Server settings in the database, editable from the dashboard. See the MySQL migration. */
CREATE TABLE IF NOT EXISTS server_config (
    file TEXT NOT NULL,
    name TEXT NOT NULL,
    file_value TEXT NULL,
    file_source TEXT NOT NULL DEFAULT '',
    web_value TEXT NULL,
    web_wins INTEGER NOT NULL DEFAULT 0,
    secret INTEGER NOT NULL DEFAULT 0,
    description TEXT NULL,
    seen_at BIGINT NOT NULL DEFAULT 0,
    updated_at BIGINT NOT NULL DEFAULT 0,
    updated_by TEXT NOT NULL DEFAULT '',
    PRIMARY KEY (file, name)
);
