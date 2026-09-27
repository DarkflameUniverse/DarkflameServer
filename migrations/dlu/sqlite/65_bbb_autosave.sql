/* bbb_autosave: see the MySQL migration. */
CREATE TABLE IF NOT EXISTS bbb_autosave (
	character_id BIGINT NOT NULL PRIMARY KEY,
	lxfml BLOB NOT NULL,
	source_items TEXT NOT NULL,
	updated_at BIGINT NOT NULL DEFAULT 0
);
