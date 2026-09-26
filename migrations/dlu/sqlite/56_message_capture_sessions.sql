/* message_capture_sessions and message_capture_entries: see the MySQL migration. */
CREATE TABLE IF NOT EXISTS message_capture_sessions (
	id INTEGER PRIMARY KEY AUTOINCREMENT,
	character_id BIGINT NOT NULL,
	character_name TEXT NOT NULL DEFAULT '',
	account_id INTEGER NOT NULL DEFAULT 0,
	account_name TEXT NOT NULL DEFAULT '',
	started_by_id INTEGER NOT NULL DEFAULT 0,
	started_by TEXT NOT NULL DEFAULT '',
	started_at BIGINT NOT NULL DEFAULT 0,
	ends_at BIGINT NOT NULL DEFAULT 0,
	ended_at BIGINT NOT NULL DEFAULT 0,
	end_reason TEXT NOT NULL DEFAULT '',
	to_server INTEGER NOT NULL DEFAULT 1,
	to_client INTEGER NOT NULL DEFAULT 1,
	only_messages TEXT NOT NULL DEFAULT '',
	skip_messages TEXT NOT NULL DEFAULT '',
	zone_id INTEGER NOT NULL DEFAULT 0,
	instance_id INTEGER NOT NULL DEFAULT 0,
	clone_id INTEGER NOT NULL DEFAULT 0,
	zones TEXT NOT NULL DEFAULT '',
	message_count BIGINT NOT NULL DEFAULT 0,
	byte_count BIGINT NOT NULL DEFAULT 0,
	dropped BIGINT NOT NULL DEFAULT 0
);
CREATE INDEX IF NOT EXISTS message_capture_sessions_character ON message_capture_sessions (character_id);
CREATE INDEX IF NOT EXISTS message_capture_sessions_account ON message_capture_sessions (account_id);
CREATE INDEX IF NOT EXISTS message_capture_sessions_started ON message_capture_sessions (started_at);

CREATE TABLE IF NOT EXISTS message_capture_entries (
	session_id BIGINT NOT NULL,
	seq INTEGER NOT NULL,
	time_ms BIGINT NOT NULL,
	direction INTEGER NOT NULL,
	message_id INTEGER NOT NULL,
	object_id BIGINT NOT NULL,
	bits INTEGER NOT NULL,
	dropped_before INTEGER NOT NULL DEFAULT 0,
	zone_id INTEGER NOT NULL DEFAULT 0,
	instance_id INTEGER NOT NULL DEFAULT 0,
	clone_id INTEGER NOT NULL DEFAULT 0,
	payload BLOB NOT NULL,
	decoded TEXT NOT NULL DEFAULT '',
	PRIMARY KEY (session_id, seq)
);
