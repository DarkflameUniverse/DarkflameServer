/* Strikes against accounts. See the MySQL migration. */
CREATE TABLE IF NOT EXISTS account_strikes (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    account_id INTEGER NOT NULL,
    character_id BIGINT NOT NULL DEFAULT 0,
    source TEXT NOT NULL,
    subject TEXT NOT NULL DEFAULT '',
    reason TEXT NOT NULL DEFAULT '',
    given_by_id INTEGER NOT NULL DEFAULT 0,
    given_by TEXT NOT NULL DEFAULT '',
    created_at BIGINT NOT NULL,
    revoked_at BIGINT NOT NULL DEFAULT 0,
    revoked_by TEXT NOT NULL DEFAULT '',
    revoke_reason TEXT NOT NULL DEFAULT ''
);
CREATE INDEX IF NOT EXISTS account_strikes_account ON account_strikes (account_id);
