/* Account email for password resets. Column names match NexusDashboard. */
ALTER TABLE accounts ADD COLUMN email TEXT DEFAULT '';
ALTER TABLE accounts ADD COLUMN email_confirmed_at DATETIME DEFAULT NULL;
/* Sessions issued before this unix time are rejected, e.g. after a password reset */
ALTER TABLE accounts ADD COLUMN sessions_valid_after BIGINT NOT NULL DEFAULT 0;
CREATE TABLE IF NOT EXISTS account_tokens (
    token_hash TEXT NOT NULL PRIMARY KEY,
    account_id INTEGER NOT NULL,
    purpose TEXT NOT NULL,
    data TEXT NOT NULL DEFAULT '',
    expires_at BIGINT NOT NULL
);
CREATE INDEX IF NOT EXISTS account_tokens_account ON account_tokens (account_id, purpose);
