/* Moderation history per account, and temporary bans with a reason. */
ALTER TABLE accounts ADD COLUMN ban_expires BIGINT NOT NULL DEFAULT 0;
ALTER TABLE accounts ADD COLUMN ban_reason TEXT NULL;
CREATE TABLE IF NOT EXISTS account_notes (
    id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
    account_id INT NOT NULL,
    kind VARCHAR(16) NOT NULL,
    text TEXT NOT NULL,
    actor VARCHAR(64) NOT NULL DEFAULT '',
    created_at BIGINT NOT NULL,
    INDEX account_notes_account (account_id, id)
);
