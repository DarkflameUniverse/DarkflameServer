/* Chat, for moderation and chat bridges. See the MySQL migration. */
CREATE TABLE IF NOT EXISTS chat_log (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    time BIGINT NOT NULL,
    channel TEXT NOT NULL,
    sender_id BIGINT NOT NULL DEFAULT 0,
    sender_name TEXT NOT NULL DEFAULT '',
    account_id INTEGER NOT NULL DEFAULT 0,
    recipient_id BIGINT NOT NULL DEFAULT 0,
    recipient_name TEXT NOT NULL DEFAULT '',
    zone_id INTEGER NOT NULL DEFAULT 0,
    instance_id INTEGER NOT NULL DEFAULT 0,
    clone_id INTEGER NOT NULL DEFAULT 0,
    message TEXT NOT NULL,
    blocked INTEGER NOT NULL DEFAULT 0
);
CREATE INDEX IF NOT EXISTS chat_log_time ON chat_log (time);
CREATE INDEX IF NOT EXISTS chat_log_sender ON chat_log (sender_id);
CREATE INDEX IF NOT EXISTS chat_log_account ON chat_log (account_id);
