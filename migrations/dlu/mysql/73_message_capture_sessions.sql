/* Saved game message inspector captures (see dDashboardServer/routes/Inspector.h). One row per capture, written when it
   starts and kept up to date while it runs; ended_at is 0 until it ends. only_messages / skip_messages: the capture's
   message ID filters, comma separated. zones: the worlds it was captured in, in order, as zone:instance:clone separated by
   spaces. byte_count: bytes stored for its messages (raw bytes plus decoded fields). The Message capture pruning task
   deletes old sessions (inspector_session_days, inspector_max_mb). */
CREATE TABLE IF NOT EXISTS message_capture_sessions (
	id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
	character_id BIGINT NOT NULL,
	character_name VARCHAR(64) NOT NULL DEFAULT '',
	account_id INT UNSIGNED NOT NULL DEFAULT 0,
	account_name VARCHAR(64) NOT NULL DEFAULT '',
	started_by_id INT UNSIGNED NOT NULL DEFAULT 0,
	started_by VARCHAR(64) NOT NULL DEFAULT '',
	started_at BIGINT NOT NULL DEFAULT 0,
	ends_at BIGINT NOT NULL DEFAULT 0,
	ended_at BIGINT NOT NULL DEFAULT 0,
	end_reason VARCHAR(255) NOT NULL DEFAULT '',
	to_server TINYINT NOT NULL DEFAULT 1,
	to_client TINYINT NOT NULL DEFAULT 1,
	only_messages TEXT NOT NULL,
	skip_messages TEXT NOT NULL,
	zone_id INT UNSIGNED NOT NULL DEFAULT 0,
	instance_id INT UNSIGNED NOT NULL DEFAULT 0,
	clone_id INT UNSIGNED NOT NULL DEFAULT 0,
	zones TEXT NOT NULL,
	message_count BIGINT UNSIGNED NOT NULL DEFAULT 0,
	byte_count BIGINT UNSIGNED NOT NULL DEFAULT 0,
	dropped BIGINT UNSIGNED NOT NULL DEFAULT 0,
	INDEX message_capture_sessions_character (character_id),
	INDEX message_capture_sessions_account (account_id),
	INDEX message_capture_sessions_started (started_at)
);

/* The messages of a saved capture. seq: the dashboard's number for the message in its session, from 1.
   dropped_before: messages the world left out just before this one (too many at once). direction: 0 sent by the client,
   1 sent to it. payload: the message's bytes after the object and message ID (at most 2 KB; bits is its full size).
   decoded: its fields as JSON when the world server had a typed struct for it, else empty. */
CREATE TABLE IF NOT EXISTS message_capture_entries (
	session_id BIGINT UNSIGNED NOT NULL,
	seq INT UNSIGNED NOT NULL,
	time_ms BIGINT NOT NULL,
	direction TINYINT UNSIGNED NOT NULL,
	message_id SMALLINT UNSIGNED NOT NULL,
	object_id BIGINT NOT NULL,
	bits INT UNSIGNED NOT NULL,
	dropped_before INT UNSIGNED NOT NULL DEFAULT 0,
	zone_id INT UNSIGNED NOT NULL DEFAULT 0,
	instance_id INT UNSIGNED NOT NULL DEFAULT 0,
	clone_id INT UNSIGNED NOT NULL DEFAULT 0,
	payload BLOB NOT NULL,
	decoded TEXT NOT NULL,
	PRIMARY KEY (session_id, seq)
);
