-- Who may edit a zone's files (docs/ZonePublish.md): each zone's assigned editors, and the one account holding its
-- lock. A lock has no expiry: its holder releases it, or an admin does.
CREATE TABLE IF NOT EXISTS zone_edit_locks (
	zone_id INTEGER NOT NULL PRIMARY KEY,
	account_id INTEGER NOT NULL REFERENCES accounts(id),
	locked_at BIGINT NOT NULL
);

CREATE TABLE IF NOT EXISTS zone_editors (
	zone_id INTEGER NOT NULL,
	account_id INTEGER NOT NULL REFERENCES accounts(id),
	assigned_by TEXT NOT NULL DEFAULT '',
	assigned_at BIGINT NOT NULL,
	PRIMARY KEY (zone_id, account_id)
);
