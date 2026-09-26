/* Every change of a setting's web value. See the MySQL migration. */
CREATE TABLE IF NOT EXISTS server_config_history (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    file TEXT NOT NULL,
    name TEXT NOT NULL,
    old_value TEXT NULL DEFAULT NULL,
    old_web_wins INTEGER NOT NULL DEFAULT 0,
    new_value TEXT NULL DEFAULT NULL,
    new_web_wins INTEGER NOT NULL DEFAULT 0,
    file_value TEXT NULL DEFAULT NULL,
    secret INTEGER NOT NULL DEFAULT 0,
    removed INTEGER NOT NULL DEFAULT 0,
    revert_of BIGINT NOT NULL DEFAULT 0,
    changed_at BIGINT NOT NULL,
    account_id INTEGER NOT NULL DEFAULT 0,
    changed_by TEXT NOT NULL DEFAULT ''
);
CREATE INDEX IF NOT EXISTS server_config_history_setting ON server_config_history (file, name, id);
