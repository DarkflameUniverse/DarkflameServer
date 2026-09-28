/* UGC server: process_cpu_ms is the CPU time the worker thread spent on the last successful make, process_memory_kb
   the memory the UGC server estimated for it (what its memory budget counts; not measured), for the dashboard. */
SET @dlu_column = (SELECT IF(COUNT(*) = 0, 'ALTER TABLE ugc ADD COLUMN process_cpu_ms INT UNSIGNED NOT NULL DEFAULT 0', 'DO 0') FROM information_schema.columns
	WHERE table_schema = DATABASE() AND table_name = 'ugc' AND column_name = 'process_cpu_ms');
PREPARE dlu_column_stmt FROM @dlu_column;
EXECUTE dlu_column_stmt;
DEALLOCATE PREPARE dlu_column_stmt;

SET @dlu_column = (SELECT IF(COUNT(*) = 0, 'ALTER TABLE ugc ADD COLUMN process_memory_kb INT UNSIGNED NOT NULL DEFAULT 0', 'DO 0') FROM information_schema.columns
	WHERE table_schema = DATABASE() AND table_name = 'ugc' AND column_name = 'process_memory_kb');
PREPARE dlu_column_stmt FROM @dlu_column;
EXECUTE dlu_column_stmt;
DEALLOCATE PREPARE dlu_column_stmt;

SET @dlu_column = (SELECT IF(COUNT(*) = 0, 'ALTER TABLE ugc_modular_build ADD COLUMN process_cpu_ms INT UNSIGNED NOT NULL DEFAULT 0', 'DO 0') FROM information_schema.columns
	WHERE table_schema = DATABASE() AND table_name = 'ugc_modular_build' AND column_name = 'process_cpu_ms');
PREPARE dlu_column_stmt FROM @dlu_column;
EXECUTE dlu_column_stmt;
DEALLOCATE PREPARE dlu_column_stmt;

SET @dlu_column = (SELECT IF(COUNT(*) = 0, 'ALTER TABLE ugc_modular_build ADD COLUMN process_memory_kb INT UNSIGNED NOT NULL DEFAULT 0', 'DO 0') FROM information_schema.columns
	WHERE table_schema = DATABASE() AND table_name = 'ugc_modular_build' AND column_name = 'process_memory_kb');
PREPARE dlu_column_stmt FROM @dlu_column;
EXECUTE dlu_column_stmt;
DEALLOCATE PREPARE dlu_column_stmt;
