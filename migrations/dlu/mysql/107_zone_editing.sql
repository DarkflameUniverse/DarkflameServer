-- Who may edit a zone's files (docs/ZonePublish.md): each zone's assigned editors, and the one account holding its
-- lock. A lock has no expiry: its holder releases it, or an admin does.
CREATE TABLE IF NOT EXISTS zone_edit_locks (
	zone_id INT UNSIGNED NOT NULL PRIMARY KEY,
	account_id INT NOT NULL REFERENCES accounts(id),
	locked_at BIGINT NOT NULL
);

CREATE TABLE IF NOT EXISTS zone_editors (
	zone_id INT UNSIGNED NOT NULL,
	account_id INT NOT NULL REFERENCES accounts(id),
	assigned_by VARCHAR(64) NOT NULL DEFAULT '',
	assigned_at BIGINT NOT NULL,
	PRIMARY KEY (zone_id, account_id)
);
