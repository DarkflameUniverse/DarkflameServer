/* Columns used by the web dashboard. Names match NexusDashboard so databases it already migrated keep their data:
   each column is only added when it is not there yet. */
SET @dlu_column = (SELECT IF(COUNT(*) = 0, 'ALTER TABLE play_keys ADD COLUMN notes TEXT NULL', 'DO 0') FROM information_schema.columns
	WHERE table_schema = DATABASE() AND table_name = 'play_keys' AND column_name = 'notes');
PREPARE dlu_column_stmt FROM @dlu_column;
EXECUTE dlu_column_stmt;
DEALLOCATE PREPARE dlu_column_stmt;
SET @dlu_column = (SELECT IF(COUNT(*) = 0, 'ALTER TABLE bug_reports ADD COLUMN resolved_time TIMESTAMP NULL DEFAULT NULL', 'DO 0') FROM information_schema.columns
	WHERE table_schema = DATABASE() AND table_name = 'bug_reports' AND column_name = 'resolved_time');
PREPARE dlu_column_stmt FROM @dlu_column;
EXECUTE dlu_column_stmt;
DEALLOCATE PREPARE dlu_column_stmt;
SET @dlu_column = (SELECT IF(COUNT(*) = 0, 'ALTER TABLE bug_reports ADD COLUMN resoleved_by_id INT NULL', 'DO 0') FROM information_schema.columns
	WHERE table_schema = DATABASE() AND table_name = 'bug_reports' AND column_name = 'resoleved_by_id');
PREPARE dlu_column_stmt FROM @dlu_column;
EXECUTE dlu_column_stmt;
DEALLOCATE PREPARE dlu_column_stmt;
SET @dlu_column = (SELECT IF(COUNT(*) = 0, 'ALTER TABLE bug_reports ADD COLUMN resolution TEXT NULL', 'DO 0') FROM information_schema.columns
	WHERE table_schema = DATABASE() AND table_name = 'bug_reports' AND column_name = 'resolution');
PREPARE dlu_column_stmt FROM @dlu_column;
EXECUTE dlu_column_stmt;
DEALLOCATE PREPARE dlu_column_stmt;
