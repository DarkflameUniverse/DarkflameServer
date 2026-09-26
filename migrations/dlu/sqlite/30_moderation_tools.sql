/* Strike thresholds, player reports and chat filter words. See the MySQL migration. */
ALTER TABLE account_strikes ADD COLUMN step TEXT NOT NULL DEFAULT '';
ALTER TABLE account_strikes ADD COLUMN step_count INTEGER NOT NULL DEFAULT 0;

CREATE TABLE IF NOT EXISTS player_reports (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    created_at BIGINT NOT NULL,
    kind TEXT NOT NULL,
    reporter_id BIGINT NOT NULL DEFAULT 0,
    reporter_account_id INTEGER NOT NULL DEFAULT 0,
    object_id BIGINT NOT NULL DEFAULT 0,
    object_lot INTEGER NOT NULL DEFAULT 0,
    target_character_id BIGINT NOT NULL DEFAULT 0,
    target_account_id INTEGER NOT NULL DEFAULT 0,
    property_id BIGINT NOT NULL DEFAULT 0,
    zone_id INTEGER NOT NULL DEFAULT 0,
    instance_id INTEGER NOT NULL DEFAULT 0,
    clone_id INTEGER NOT NULL DEFAULT 0,
    body TEXT NOT NULL DEFAULT '',
    status INTEGER NOT NULL DEFAULT 0,
    handled_by TEXT NOT NULL DEFAULT '',
    handled_at BIGINT NOT NULL DEFAULT 0,
    resolution TEXT NOT NULL DEFAULT ''
);
CREATE INDEX IF NOT EXISTS player_reports_status ON player_reports (status, id);
CREATE INDEX IF NOT EXISTS player_reports_target ON player_reports (target_account_id);

CREATE TABLE IF NOT EXISTS chat_filter_words (
    word TEXT NOT NULL PRIMARY KEY,
    allowed INTEGER NOT NULL,
    added_by TEXT NOT NULL DEFAULT '',
    added_at BIGINT NOT NULL
);
