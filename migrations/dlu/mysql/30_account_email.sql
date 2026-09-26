/* Account email for password resets. email/email_confirmed_at match NexusDashboard's columns so existing data is kept,
   statements for columns that already exist fail individually and are skipped. */
ALTER TABLE accounts ADD COLUMN email VARCHAR(255) NULL DEFAULT '';
ALTER TABLE accounts ADD COLUMN email_confirmed_at DATETIME NULL DEFAULT NULL;
/* Sessions issued before this unix time are rejected, e.g. after a password reset */
ALTER TABLE accounts ADD COLUMN sessions_valid_after BIGINT NOT NULL DEFAULT 0;
CREATE TABLE IF NOT EXISTS account_tokens (
    token_hash CHAR(64) NOT NULL PRIMARY KEY,
    account_id INT NOT NULL,
    purpose VARCHAR(16) NOT NULL,
    data VARCHAR(255) NOT NULL DEFAULT '',
    expires_at BIGINT NOT NULL,
    INDEX account_tokens_account (account_id, purpose)
);
