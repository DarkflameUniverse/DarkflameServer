/* ugc.priority: models staff asked to be made again. See the MySQL migration. */
ALTER TABLE ugc ADD COLUMN priority INTEGER NOT NULL DEFAULT 0;
