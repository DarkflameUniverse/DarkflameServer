/* Account email for password resets. email/email_confirmed_at match NexusDashboard's columns so existing data is kept:
   they are only added when they are not there yet. */
SET @dlu_column = (SELECT IF(COUNT(*) = 0, 'ALTER TABLE accounts ADD COLUMN email VARCHAR(255) NULL DEFAULT ''''', 'DO 0') FROM information_schema.columns
	WHERE table_schema = DATABASE() AND table_name = 'accounts' AND column_name = 'email');
PREPARE dlu_column_stmt FROM @dlu_column;
EXECUTE dlu_column_stmt;
DEALLOCATE PREPARE dlu_column_stmt;
SET @dlu_column = (SELECT IF(COUNT(*) = 0, 'ALTER TABLE accounts ADD COLUMN email_confirmed_at DATETIME NULL DEFAULT NULL', 'DO 0') FROM information_schema.columns
	WHERE table_schema = DATABASE() AND table_name = 'accounts' AND column_name = 'email_confirmed_at');
PREPARE dlu_column_stmt FROM @dlu_column;
EXECUTE dlu_column_stmt;
DEALLOCATE PREPARE dlu_column_stmt;
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
