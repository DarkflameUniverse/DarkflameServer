/* ugc.priority: models staff asked to be made again (/reprocessproperty, the dashboard's Reprocess all models). The
   UGC server makes them before any other model; cleared once made. */
SET @dlu_column = (SELECT IF(COUNT(*) = 0, 'ALTER TABLE ugc ADD COLUMN priority TINYINT NOT NULL DEFAULT 0', 'DO 0') FROM information_schema.columns
	WHERE table_schema = DATABASE() AND table_name = 'ugc' AND column_name = 'priority');
PREPARE dlu_column_stmt FROM @dlu_column;
EXECUTE dlu_column_stmt;
DEALLOCATE PREPARE dlu_column_stmt;
