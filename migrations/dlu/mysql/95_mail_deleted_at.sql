/* mail.deleted_at: when the player deleted the mail in game (unix time; 0 = not deleted). Deleted mail stays for staff
   to read on the dashboard; the game never shows it again. */
SET @dlu_column = (SELECT IF(COUNT(*) = 0, 'ALTER TABLE mail ADD COLUMN deleted_at BIGINT NOT NULL DEFAULT 0', 'DO 0') FROM information_schema.columns
	WHERE table_schema = DATABASE() AND table_name = 'mail' AND column_name = 'deleted_at');
PREPARE dlu_column_stmt FROM @dlu_column;
EXECUTE dlu_column_stmt;
DEALLOCATE PREPARE dlu_column_stmt;
