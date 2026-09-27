/* UGC server: the quiet period after a save, and icon settings presets and overrides. See the MySQL migration. */
ALTER TABLE ugc ADD COLUMN process_after BIGINT NOT NULL DEFAULT 0;

CREATE TABLE IF NOT EXISTS ugc_icon_settings (
	target TEXT NOT NULL PRIMARY KEY,
	params TEXT NOT NULL,
	updated_at BIGINT NOT NULL DEFAULT 0
);
