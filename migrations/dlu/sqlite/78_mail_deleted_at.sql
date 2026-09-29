/* mail.deleted_at: when the player deleted the mail in game. See the MySQL migration. */
ALTER TABLE mail ADD COLUMN deleted_at BIGINT NOT NULL DEFAULT 0;
