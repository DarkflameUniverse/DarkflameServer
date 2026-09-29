/* Chat flags, the chat_log rows they cover and their history. See the MySQL migration. */
CREATE TABLE IF NOT EXISTS chat_flags (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    created_at BIGINT NOT NULL,
    created_by_id INTEGER NOT NULL DEFAULT 0,
    created_by TEXT NOT NULL DEFAULT '',
    status TEXT NOT NULL DEFAULT 'open',
    channel TEXT NOT NULL,
    character_id BIGINT NOT NULL DEFAULT 0,
    character_name TEXT NOT NULL DEFAULT '',
    account_id INTEGER NOT NULL DEFAULT 0,
    first_message_id BIGINT NOT NULL DEFAULT 0,
    last_message_id BIGINT NOT NULL DEFAULT 0,
    first_time BIGINT NOT NULL DEFAULT 0,
    last_time BIGINT NOT NULL DEFAULT 0,
    excerpt TEXT NOT NULL DEFAULT '',
    note TEXT NOT NULL DEFAULT '',
    messages TEXT NOT NULL DEFAULT '[]',
    player_report_id BIGINT NOT NULL DEFAULT 0,
    updated_at BIGINT NOT NULL DEFAULT 0,
    updated_by TEXT NOT NULL DEFAULT ''
);
CREATE INDEX IF NOT EXISTS chat_flags_status ON chat_flags (status);
CREATE INDEX IF NOT EXISTS chat_flags_character ON chat_flags (character_id);
CREATE INDEX IF NOT EXISTS chat_flags_account ON chat_flags (account_id);

CREATE TABLE IF NOT EXISTS chat_flag_messages (
    flag_id BIGINT NOT NULL,
    message_id BIGINT NOT NULL,
    PRIMARY KEY (flag_id, message_id)
);
CREATE INDEX IF NOT EXISTS chat_flag_messages_message ON chat_flag_messages (message_id);

CREATE TABLE IF NOT EXISTS chat_flag_events (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    flag_id BIGINT NOT NULL,
    time BIGINT NOT NULL,
    account_id INTEGER NOT NULL DEFAULT 0,
    actor TEXT NOT NULL DEFAULT '',
    action TEXT NOT NULL,
    detail TEXT NOT NULL DEFAULT ''
);
CREATE INDEX IF NOT EXISTS chat_flag_events_flag ON chat_flag_events (flag_id);
