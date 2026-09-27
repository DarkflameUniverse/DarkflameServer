/* Dashboard API keys. See the MySQL migration. */
CREATE TABLE IF NOT EXISTS dashboard_api_keys (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    account_id INTEGER NOT NULL,
    name TEXT NOT NULL,
    note TEXT NOT NULL DEFAULT '',
    key_hash TEXT NOT NULL,
    key_prefix TEXT NOT NULL,
    permissions TEXT NOT NULL DEFAULT '',
    read_only INTEGER NOT NULL DEFAULT 0,
    allowed_ips TEXT NOT NULL DEFAULT '',
    allowed_paths TEXT NOT NULL DEFAULT '',
    rate_limit INTEGER NOT NULL DEFAULT 0,
    daily_quota INTEGER NOT NULL DEFAULT 0,
    created_at BIGINT NOT NULL,
    created_by TEXT NOT NULL DEFAULT '',
    issued_at BIGINT NOT NULL,
    expires_at BIGINT NOT NULL DEFAULT 0,
    revoked_at BIGINT NOT NULL DEFAULT 0,
    revoked_by TEXT NOT NULL DEFAULT '',
    last_used_at BIGINT NOT NULL DEFAULT 0,
    last_ip TEXT NOT NULL DEFAULT '',
    request_count BIGINT NOT NULL DEFAULT 0,
    quota_day INTEGER NOT NULL DEFAULT 0,
    day_count INTEGER NOT NULL DEFAULT 0
);
CREATE UNIQUE INDEX IF NOT EXISTS dashboard_api_keys_hash ON dashboard_api_keys (key_hash);
CREATE INDEX IF NOT EXISTS dashboard_api_keys_account ON dashboard_api_keys (account_id);
