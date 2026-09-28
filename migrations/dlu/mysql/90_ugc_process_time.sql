/* UGC server: ugc.process_ms and ugc_modular_build.process_ms are how long the last successful make took, in
   milliseconds (0 until it has made it), shown on the dashboard's UGC page. */
SET @dlu_column = (SELECT IF(COUNT(*) = 0, 'ALTER TABLE ugc ADD COLUMN process_ms INT UNSIGNED NOT NULL DEFAULT 0', 'DO 0') FROM information_schema.columns
	WHERE table_schema = DATABASE() AND table_name = 'ugc' AND column_name = 'process_ms');
PREPARE dlu_column_stmt FROM @dlu_column;
EXECUTE dlu_column_stmt;
DEALLOCATE PREPARE dlu_column_stmt;

SET @dlu_column = (SELECT IF(COUNT(*) = 0, 'ALTER TABLE ugc_modular_build ADD COLUMN process_ms INT UNSIGNED NOT NULL DEFAULT 0', 'DO 0') FROM information_schema.columns
	WHERE table_schema = DATABASE() AND table_name = 'ugc_modular_build' AND column_name = 'process_ms');
PREPARE dlu_column_stmt FROM @dlu_column;
EXECUTE dlu_column_stmt;
DEALLOCATE PREPARE dlu_column_stmt;
