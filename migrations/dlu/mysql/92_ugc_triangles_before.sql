/* UGC server: ugc.triangle_count_before is the most detailed level's triangles before the UGC server removed the faces
   that can't be seen (0 until known), so the dashboard can show how much each model saved (with triangle_count). */
SET @dlu_column = (SELECT IF(COUNT(*) = 0, 'ALTER TABLE ugc ADD COLUMN triangle_count_before INT UNSIGNED NOT NULL DEFAULT 0', 'DO 0') FROM information_schema.columns
	WHERE table_schema = DATABASE() AND table_name = 'ugc' AND column_name = 'triangle_count_before');
PREPARE dlu_column_stmt FROM @dlu_column;
EXECUTE dlu_column_stmt;
DEALLOCATE PREPARE dlu_column_stmt;
