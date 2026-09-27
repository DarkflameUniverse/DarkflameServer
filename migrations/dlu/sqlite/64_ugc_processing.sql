/* UGC server state: see the MySQL migration. */
ALTER TABLE ugc ADD COLUMN processed_at BIGINT NOT NULL DEFAULT 0;
ALTER TABLE ugc ADD COLUMN process_attempts INTEGER NOT NULL DEFAULT 0;
ALTER TABLE ugc ADD COLUMN process_error TEXT NOT NULL DEFAULT '';
ALTER TABLE ugc_modular_build ADD COLUMN is_optimized INTEGER NOT NULL DEFAULT 0;
ALTER TABLE ugc_modular_build ADD COLUMN processed_at BIGINT NOT NULL DEFAULT 0;
ALTER TABLE ugc_modular_build ADD COLUMN process_attempts INTEGER NOT NULL DEFAULT 0;
ALTER TABLE ugc_modular_build ADD COLUMN process_error TEXT NOT NULL DEFAULT '';
CREATE INDEX IF NOT EXISTS ugc_is_optimized ON ugc (is_optimized);
CREATE INDEX IF NOT EXISTS ugc_modular_build_is_optimized ON ugc_modular_build (is_optimized);
