/* Moderation history per account, and temporary bans. See the MySQL migration. */
ALTER TABLE accounts ADD COLUMN ban_expires BIGINT NOT NULL DEFAULT 0;
ALTER TABLE accounts ADD COLUMN ban_reason TEXT NULL;
CREATE TABLE IF NOT EXISTS account_notes (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    account_id INTEGER NOT NULL,
    kind TEXT NOT NULL,
    text TEXT NOT NULL,
    actor TEXT NOT NULL DEFAULT '',
    created_at BIGINT NOT NULL
);
CREATE INDEX IF NOT EXISTS account_notes_account ON account_notes (account_id, id);
