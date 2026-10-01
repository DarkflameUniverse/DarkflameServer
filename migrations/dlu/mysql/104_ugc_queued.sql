/* ugc.queued: whether a model may be made while the UGC setting queue_on_property_save is on (docs/UgcServer.md,
   "Queue on property save"). Set when a model is saved from Brick By Brick, made again by staff, or its property is
   saved. Models from before stay 0, so turning the setting on doesn't queue a whole server's backlog at once. */
SET @dlu_column = (SELECT IF(COUNT(*) = 0, 'ALTER TABLE ugc ADD COLUMN queued TINYINT NOT NULL DEFAULT 0', 'DO 0') FROM information_schema.columns
	WHERE table_schema = DATABASE() AND table_name = 'ugc' AND column_name = 'queued');
PREPARE dlu_column_stmt FROM @dlu_column;
EXECUTE dlu_column_stmt;
DEALLOCATE PREPARE dlu_column_stmt;
