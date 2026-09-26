/* Outgoing webhooks, economy anomaly flags, small persistent dashboard state and two-factor login. */
CREATE TABLE IF NOT EXISTS dashboard_webhooks (
    id INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
    name VARCHAR(64) NOT NULL,
    url TEXT NOT NULL,
    format VARCHAR(16) NOT NULL DEFAULT 'discord',
    events TEXT NOT NULL,
    secret VARCHAR(128) NOT NULL DEFAULT '',
    enabled TINYINT NOT NULL DEFAULT 1,
    created_at BIGINT NOT NULL,
    last_sent_at BIGINT NOT NULL DEFAULT 0,
    last_status INT NOT NULL DEFAULT 0,
    last_error TEXT NULL
);
/* kind: 1 = unusual coin income, 2 = unusual item creation, 3 = duplicated object id (day 0, so it is flagged once). */
CREATE TABLE IF NOT EXISTS economy_flags (
    id BIGINT NOT NULL AUTO_INCREMENT PRIMARY KEY,
    created_at BIGINT NOT NULL,
    day INT NOT NULL,
    kind TINYINT NOT NULL,
    character_id BIGINT NOT NULL DEFAULT 0,
    lot INT NOT NULL DEFAULT 0,
    item_id BIGINT NOT NULL DEFAULT 0,
    value BIGINT NOT NULL DEFAULT 0,
    baseline BIGINT NOT NULL DEFAULT 0,
    details TEXT NULL,
    status TINYINT NOT NULL DEFAULT 0,
    reviewed_by INT NOT NULL DEFAULT 0,
    reviewed_at BIGINT NOT NULL DEFAULT 0,
    note TEXT NULL,
    UNIQUE KEY economy_flags_subject (kind, day, character_id, lot, item_id),
    INDEX economy_flags_status (status, id)
);
CREATE TABLE IF NOT EXISTS dashboard_state (
    name VARCHAR(64) NOT NULL PRIMARY KEY,
    value TEXT NOT NULL
);
/* totp_secret is encrypted with the dashboard's own key. totp_enabled_at is 0 while two-factor login is off. */
ALTER TABLE accounts ADD COLUMN totp_secret TEXT NULL;
ALTER TABLE accounts ADD COLUMN totp_enabled_at BIGINT NOT NULL DEFAULT 0;
ALTER TABLE accounts ADD COLUMN totp_last_step BIGINT NOT NULL DEFAULT 0;
CREATE TABLE IF NOT EXISTS account_recovery_codes (
    id INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
    account_id INT NOT NULL,
    code_hash VARCHAR(64) NOT NULL,
    used_at BIGINT NOT NULL DEFAULT 0,
    INDEX account_recovery_codes_account (account_id)
);
