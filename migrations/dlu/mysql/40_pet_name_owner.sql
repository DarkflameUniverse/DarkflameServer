/* Who owns each named pet, written by the game when the name is set. NULL: not looked up yet (the dashboard fills
   those in once from the characters' inventories); 0: no character has the pet any more. */
SET @dlu_column = (SELECT IF(COUNT(*) = 0, 'ALTER TABLE pet_names ADD COLUMN owner_id BIGINT NULL DEFAULT NULL', 'DO 0') FROM information_schema.columns
	WHERE table_schema = DATABASE() AND table_name = 'pet_names' AND column_name = 'owner_id');
PREPARE dlu_column_stmt FROM @dlu_column;
EXECUTE dlu_column_stmt;
DEALLOCATE PREPARE dlu_column_stmt;
/* Some servers added this column (and an index on it) themselves: only index it when nothing does yet */
SET @dlu_index = (SELECT IF(COUNT(*) = 0, 'CREATE INDEX pet_names_owner ON pet_names (owner_id)', 'DO 0') FROM information_schema.statistics
	WHERE table_schema = DATABASE() AND table_name = 'pet_names' AND column_name = 'owner_id' AND seq_in_index = 1);
PREPARE dlu_index_stmt FROM @dlu_index;
EXECUTE dlu_index_stmt;
DEALLOCATE PREPARE dlu_index_stmt;
/* Those servers' character ids written before 23_store_character_id_as_objectid.sql, which did not convert this
   column, have no persistent bit and match no character. Give them the bit, as 23 did for every other character id
   column. Does nothing on a column just added (every row is NULL). */
UPDATE pet_names SET owner_id = owner_id | 0x1000000000000000 WHERE owner_id > 0 AND owner_id < 0x1000000000000000;
