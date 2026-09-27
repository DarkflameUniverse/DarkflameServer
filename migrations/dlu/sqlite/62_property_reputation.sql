/* property_reputation_visits: see the MySQL migration. */
CREATE TABLE IF NOT EXISTS property_reputation_visits (
	property_id BIGINT NOT NULL,
	account_id INTEGER NOT NULL,
	day INTEGER NOT NULL,
	points BIGINT NOT NULL DEFAULT 0,
	seconds BIGINT NOT NULL DEFAULT 0,
	PRIMARY KEY (property_id, account_id, day)
);
CREATE INDEX IF NOT EXISTS property_reputation_visits_day ON property_reputation_visits (property_id, day);
