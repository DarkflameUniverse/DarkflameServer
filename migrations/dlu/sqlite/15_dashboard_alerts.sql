/* Outgoing webhooks, economy anomaly flags, dashboard state and two-factor login. See the MySQL migration. */
CREATE TABLE IF NOT EXISTS dashboard_webhooks (
    id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    url TEXT NOT NULL,
    format TEXT NOT NULL DEFAULT 'discord',
    events TEXT NOT NULL,
    secret TEXT NOT NULL DEFAULT '',
    enabled INTEGER NOT NULL DEFAULT 1,
    created_at BIGINT NOT NULL,
    last_sent_at BIGINT NOT NULL DEFAULT 0,
    last_status INTEGER NOT NULL DEFAULT 0,
    last_error TEXT NULL
);
CREATE TABLE IF NOT EXISTS economy_flags (
    id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
    created_at BIGINT NOT NULL,
    day INTEGER NOT NULL,
    kind INTEGER NOT NULL,
    character_id BIGINT NOT NULL DEFAULT 0,
    lot INTEGER NOT NULL DEFAULT 0,
    item_id BIGINT NOT NULL DEFAULT 0,
    value BIGINT NOT NULL DEFAULT 0,
    baseline BIGINT NOT NULL DEFAULT 0,
    details TEXT NULL,
    status INTEGER NOT NULL DEFAULT 0,
    reviewed_by INTEGER NOT NULL DEFAULT 0,
    reviewed_at BIGINT NOT NULL DEFAULT 0,
    note TEXT NULL,
    UNIQUE (kind, day, character_id, lot, item_id)
);
CREATE INDEX IF NOT EXISTS economy_flags_status ON economy_flags (status, id);
CREATE TABLE IF NOT EXISTS dashboard_state (
    name TEXT NOT NULL PRIMARY KEY,
    value TEXT NOT NULL
);
ALTER TABLE accounts ADD COLUMN totp_secret TEXT NULL;
ALTER TABLE accounts ADD COLUMN totp_enabled_at BIGINT NOT NULL DEFAULT 0;
ALTER TABLE accounts ADD COLUMN totp_last_step BIGINT NOT NULL DEFAULT 0;
CREATE TABLE IF NOT EXISTS account_recovery_codes (
    id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
    account_id INTEGER NOT NULL,
    code_hash TEXT NOT NULL,
    used_at BIGINT NOT NULL DEFAULT 0
);
CREATE INDEX IF NOT EXISTS account_recovery_codes_account ON account_recovery_codes (account_id);
