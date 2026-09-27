/* Stale save guard: bumped whenever a world loads a character to play it, a world saves it or the dashboard writes it.
   A world only saves over the generation it loaded or last saved; an older world's save is refused. Only added when
   it is not there yet. */
SET @dlu_column = (SELECT IF(COUNT(*) = 0, 'ALTER TABLE charxml ADD COLUMN save_generation BIGINT NOT NULL DEFAULT 0', 'DO 0') FROM information_schema.columns
	WHERE table_schema = DATABASE() AND table_name = 'charxml' AND column_name = 'save_generation');
PREPARE dlu_column_stmt FROM @dlu_column;
EXECUTE dlu_column_stmt;
DEALLOCATE PREPARE dlu_column_stmt;
