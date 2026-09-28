/* The character who placed a property model (NULL: the owner). See the MySQL migration. */
ALTER TABLE properties_contents ADD COLUMN placed_by BIGINT DEFAULT NULL;
