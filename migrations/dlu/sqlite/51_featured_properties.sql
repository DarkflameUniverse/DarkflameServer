/* featured_properties: see the MySQL migration. */
CREATE TABLE IF NOT EXISTS featured_properties (
	template_id INTEGER NOT NULL PRIMARY KEY,
	mode INTEGER NOT NULL DEFAULT 0,
	property_id BIGINT NOT NULL DEFAULT 0,
	updated_at BIGINT NOT NULL DEFAULT 0,
	updated_by TEXT NOT NULL DEFAULT ''
);
