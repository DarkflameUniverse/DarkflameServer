/* properties_contents.placed_by is the character who placed the model (NULL: the property's owner, and every model
   placed before this column). A model picked up by another player who can build there goes back to them. */
SET @dlu_column = (SELECT IF(COUNT(*) = 0, 'ALTER TABLE properties_contents ADD COLUMN placed_by BIGINT NULL DEFAULT NULL', 'DO 0') FROM information_schema.columns
	WHERE table_schema = DATABASE() AND table_name = 'properties_contents' AND column_name = 'placed_by');
PREPARE dlu_column_stmt FROM @dlu_column;
EXECUTE dlu_column_stmt;
DEALLOCATE PREPARE dlu_column_stmt;
