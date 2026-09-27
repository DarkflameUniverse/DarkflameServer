/* property_rent_rates: see the MySQL migration. */
CREATE TABLE IF NOT EXISTS property_rent_rates (
	map_id INTEGER NOT NULL PRIMARY KEY,
	price BIGINT NOT NULL DEFAULT 0,
	period_days INTEGER NOT NULL DEFAULT 0,
	updated_by TEXT NOT NULL DEFAULT '',
	updated_at BIGINT NOT NULL DEFAULT 0
);
