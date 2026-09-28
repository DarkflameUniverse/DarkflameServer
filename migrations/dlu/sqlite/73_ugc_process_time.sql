/* UGC server: how long it took to make each model and car or rocket build. See the MySQL migration. */
ALTER TABLE ugc ADD COLUMN process_ms INTEGER NOT NULL DEFAULT 0;
ALTER TABLE ugc_modular_build ADD COLUMN process_ms INTEGER NOT NULL DEFAULT 0;
