/* AI moderator helper suggestions (staff only). See the MySQL migration. */
CREATE TABLE IF NOT EXISTS ai_suggestions (
    id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
    kind TEXT NOT NULL,
    item_id BIGINT NOT NULL,
    fingerprint TEXT NOT NULL,
    requested_by_id INTEGER NOT NULL DEFAULT 0,
    requested_by TEXT NOT NULL DEFAULT '',
    created_at BIGINT NOT NULL,
    model TEXT NOT NULL DEFAULT '',
    input_tokens INTEGER NOT NULL DEFAULT 0,
    output_tokens INTEGER NOT NULL DEFAULT 0,
    status INTEGER NOT NULL DEFAULT 0,
    suggestion TEXT NULL,
    error TEXT NULL,
    context TEXT NULL
);
CREATE INDEX IF NOT EXISTS ai_suggestions_item ON ai_suggestions (kind, item_id, id);
CREATE INDEX IF NOT EXISTS ai_suggestions_time ON ai_suggestions (created_at);
