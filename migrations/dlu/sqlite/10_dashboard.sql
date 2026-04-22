CREATE TABLE IF NOT EXISTS dashboard_audit_log (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp INTEGER NOT NULL,
    ip_address TEXT NOT NULL,
    endpoint TEXT NOT NULL,
    method TEXT NOT NULL,
    user_agent TEXT,
    response_code INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS dashboard_config (
    config_key TEXT PRIMARY KEY,
    config_value TEXT NOT NULL
);

INSERT OR IGNORE INTO dashboard_config (config_key, config_value) VALUES ('rate_limit_enabled', '1');
INSERT OR IGNORE INTO dashboard_config (config_key, config_value) VALUES ('rate_limit_requests', '100');
INSERT OR IGNORE INTO dashboard_config (config_key, config_value) VALUES ('rate_limit_window', '60');
INSERT OR IGNORE INTO dashboard_config (config_key, config_value) VALUES ('audit_retention_days', '90');
