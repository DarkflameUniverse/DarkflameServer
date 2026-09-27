/* capture_kind and capture_target: see the MySQL migration. */
ALTER TABLE message_capture_sessions ADD COLUMN capture_kind INTEGER NOT NULL DEFAULT 0;
ALTER TABLE message_capture_sessions ADD COLUMN capture_target TEXT NOT NULL DEFAULT '';
