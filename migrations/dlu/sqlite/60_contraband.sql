/* contraband_items: see the MySQL migration. */
CREATE TABLE IF NOT EXISTS contraband_items (
	lot INTEGER NOT NULL PRIMARY KEY,
	reason TEXT NOT NULL,
	action INTEGER NOT NULL DEFAULT 0,
	added_by TEXT NOT NULL DEFAULT '',
	added_at BIGINT NOT NULL DEFAULT 0
);
