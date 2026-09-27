/* Which kind of pet (its LOT) each named pet is, written by the game when the name is set and filled in for older
   rows when the owner next loads into a world. NULL or 0: not known yet. Only added when it is not there yet. */
SET @dlu_column = (SELECT IF(COUNT(*) = 0, 'ALTER TABLE pet_names ADD COLUMN pet_lot INT NULL DEFAULT NULL', 'DO 0') FROM information_schema.columns
	WHERE table_schema = DATABASE() AND table_name = 'pet_names' AND column_name = 'pet_lot');
PREPARE dlu_column_stmt FROM @dlu_column;
EXECUTE dlu_column_stmt;
DEALLOCATE PREPARE dlu_column_stmt;
