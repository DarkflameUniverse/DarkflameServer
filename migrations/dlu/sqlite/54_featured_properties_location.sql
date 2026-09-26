/* featured_properties.zone_id and featured_properties_settings: see the MySQL migration. */
ALTER TABLE featured_properties ADD COLUMN zone_id INTEGER NOT NULL DEFAULT 0;
CREATE TABLE IF NOT EXISTS featured_properties_settings (
	id INTEGER NOT NULL PRIMARY KEY,
	full_auto INTEGER NOT NULL DEFAULT 0,
	updated_at BIGINT NOT NULL DEFAULT 0,
	updated_by TEXT NOT NULL DEFAULT ''
);
