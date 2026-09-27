/* UGC server: ugc.process_after is the Unix time before which a saved model isn't made (the quiet period after a
   save, so a model the owner keeps editing isn't made for every save; 0: right away). ugc_icon_settings holds
   icon framing and lighting set on the dashboard: presets per kind ("kind:model", "kind:build6") and overrides for one
   model ("model:<id>") or one combination of car or rocket modules ("combo:<key>"). See docs/UgcServer.md. */
SET @dlu_column = (SELECT IF(COUNT(*) = 0, 'ALTER TABLE ugc ADD COLUMN process_after BIGINT NOT NULL DEFAULT 0', 'DO 0') FROM information_schema.columns
	WHERE table_schema = DATABASE() AND table_name = 'ugc' AND column_name = 'process_after');
PREPARE dlu_column_stmt FROM @dlu_column;
EXECUTE dlu_column_stmt;
DEALLOCATE PREPARE dlu_column_stmt;

CREATE TABLE IF NOT EXISTS ugc_icon_settings (
	target VARCHAR(191) NOT NULL PRIMARY KEY,
	params TEXT NOT NULL,
	updated_at BIGINT NOT NULL DEFAULT 0
);
