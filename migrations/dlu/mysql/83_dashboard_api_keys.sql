/* Dashboard API keys. Only the SHA-256 of the key is kept (key_hash); key_prefix is its start, shown in the list so
   people can tell their keys apart. permissions is '*' (all of the owner's permissions, whatever they are at the
   time) or a comma-separated list of permission names; a key never does more than its owner can right now.
   allowed_ips / allowed_paths: comma-separated, empty for any. rate_limit: requests a minute (0: the server's
   default); daily_quota: requests a UTC day (0: no quota). The usage columns are written in batches, not per request:
   quota_day is the UTC day (days since 1970) day_count counts. issued_at is when the current secret was made
   (rotating replaces the secret). */
CREATE TABLE IF NOT EXISTS dashboard_api_keys (
    id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
    account_id INT UNSIGNED NOT NULL,
    name VARCHAR(64) NOT NULL,
    note VARCHAR(255) NOT NULL DEFAULT '',
    key_hash CHAR(64) NOT NULL,
    key_prefix VARCHAR(16) NOT NULL,
    permissions TEXT NOT NULL,
    read_only TINYINT NOT NULL DEFAULT 0,
    allowed_ips VARCHAR(512) NOT NULL DEFAULT '',
    allowed_paths VARCHAR(512) NOT NULL DEFAULT '',
    rate_limit INT UNSIGNED NOT NULL DEFAULT 0,
    daily_quota INT UNSIGNED NOT NULL DEFAULT 0,
    created_at BIGINT NOT NULL,
    created_by VARCHAR(64) NOT NULL DEFAULT '',
    issued_at BIGINT NOT NULL,
    expires_at BIGINT NOT NULL DEFAULT 0,
    revoked_at BIGINT NOT NULL DEFAULT 0,
    revoked_by VARCHAR(64) NOT NULL DEFAULT '',
    last_used_at BIGINT NOT NULL DEFAULT 0,
    last_ip VARCHAR(64) NOT NULL DEFAULT '',
    request_count BIGINT UNSIGNED NOT NULL DEFAULT 0,
    quota_day INT NOT NULL DEFAULT 0,
    day_count INT UNSIGNED NOT NULL DEFAULT 0,
    UNIQUE INDEX dashboard_api_keys_hash (key_hash),
    INDEX dashboard_api_keys_account (account_id)
);
