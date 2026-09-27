/* UGC server state (see docs/UgcServer.md). ugc.is_optimized: 0 waiting to be made, 1 made, 2 failed; processed_at is
   the last attempt (Unix seconds), process_attempts how many there have been and process_error the last failure. The
   same columns on ugc_modular_build for the builds' icons. Columns are only added when they are not there yet. */
SET @dlu_column = (SELECT IF(COUNT(*) = 0, 'ALTER TABLE ugc ADD COLUMN processed_at BIGINT NOT NULL DEFAULT 0', 'DO 0') FROM information_schema.columns
	WHERE table_schema = DATABASE() AND table_name = 'ugc' AND column_name = 'processed_at');
PREPARE dlu_column_stmt FROM @dlu_column;
EXECUTE dlu_column_stmt;
DEALLOCATE PREPARE dlu_column_stmt;

SET @dlu_column = (SELECT IF(COUNT(*) = 0, 'ALTER TABLE ugc ADD COLUMN process_attempts INT NOT NULL DEFAULT 0', 'DO 0') FROM information_schema.columns
	WHERE table_schema = DATABASE() AND table_name = 'ugc' AND column_name = 'process_attempts');
PREPARE dlu_column_stmt FROM @dlu_column;
EXECUTE dlu_column_stmt;
DEALLOCATE PREPARE dlu_column_stmt;

SET @dlu_column = (SELECT IF(COUNT(*) = 0, 'ALTER TABLE ugc ADD COLUMN process_error VARCHAR(1024) NOT NULL DEFAULT ''''', 'DO 0') FROM information_schema.columns
	WHERE table_schema = DATABASE() AND table_name = 'ugc' AND column_name = 'process_error');
PREPARE dlu_column_stmt FROM @dlu_column;
EXECUTE dlu_column_stmt;
DEALLOCATE PREPARE dlu_column_stmt;

SET @dlu_column = (SELECT IF(COUNT(*) = 0, 'ALTER TABLE ugc_modular_build ADD COLUMN is_optimized INT NOT NULL DEFAULT 0', 'DO 0') FROM information_schema.columns
	WHERE table_schema = DATABASE() AND table_name = 'ugc_modular_build' AND column_name = 'is_optimized');
PREPARE dlu_column_stmt FROM @dlu_column;
EXECUTE dlu_column_stmt;
DEALLOCATE PREPARE dlu_column_stmt;

SET @dlu_column = (SELECT IF(COUNT(*) = 0, 'ALTER TABLE ugc_modular_build ADD COLUMN processed_at BIGINT NOT NULL DEFAULT 0', 'DO 0') FROM information_schema.columns
	WHERE table_schema = DATABASE() AND table_name = 'ugc_modular_build' AND column_name = 'processed_at');
PREPARE dlu_column_stmt FROM @dlu_column;
EXECUTE dlu_column_stmt;
DEALLOCATE PREPARE dlu_column_stmt;

SET @dlu_column = (SELECT IF(COUNT(*) = 0, 'ALTER TABLE ugc_modular_build ADD COLUMN process_attempts INT NOT NULL DEFAULT 0', 'DO 0') FROM information_schema.columns
	WHERE table_schema = DATABASE() AND table_name = 'ugc_modular_build' AND column_name = 'process_attempts');
PREPARE dlu_column_stmt FROM @dlu_column;
EXECUTE dlu_column_stmt;
DEALLOCATE PREPARE dlu_column_stmt;

SET @dlu_column = (SELECT IF(COUNT(*) = 0, 'ALTER TABLE ugc_modular_build ADD COLUMN process_error VARCHAR(1024) NOT NULL DEFAULT ''''', 'DO 0') FROM information_schema.columns
	WHERE table_schema = DATABASE() AND table_name = 'ugc_modular_build' AND column_name = 'process_error');
PREPARE dlu_column_stmt FROM @dlu_column;
EXECUTE dlu_column_stmt;
DEALLOCATE PREPARE dlu_column_stmt;

/* What the UGC server looks for */
SET @dlu_index = (SELECT IF(COUNT(*) = 0, 'CREATE INDEX ugc_is_optimized ON ugc (is_optimized)', 'DO 0') FROM information_schema.statistics
	WHERE table_schema = DATABASE() AND table_name = 'ugc' AND index_name = 'ugc_is_optimized');
PREPARE dlu_index_stmt FROM @dlu_index;
EXECUTE dlu_index_stmt;
DEALLOCATE PREPARE dlu_index_stmt;

SET @dlu_index = (SELECT IF(COUNT(*) = 0, 'CREATE INDEX ugc_modular_build_is_optimized ON ugc_modular_build (is_optimized)', 'DO 0') FROM information_schema.statistics
	WHERE table_schema = DATABASE() AND table_name = 'ugc_modular_build' AND index_name = 'ugc_modular_build_is_optimized');
PREPARE dlu_index_stmt FROM @dlu_index;
EXECUTE dlu_index_stmt;
DEALLOCATE PREPARE dlu_index_stmt;
