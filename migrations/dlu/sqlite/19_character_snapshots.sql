/* Earlier versions of characters' XML. See the MySQL migration. */
CREATE TABLE IF NOT EXISTS character_snapshots (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    character_id BIGINT NOT NULL,
    taken_at BIGINT NOT NULL,
    reason TEXT NOT NULL DEFAULT '',
    actor TEXT NOT NULL DEFAULT '',
    size INTEGER NOT NULL DEFAULT 0,
    hash TEXT NOT NULL DEFAULT '',
    xml BLOB NOT NULL
);
CREATE INDEX IF NOT EXISTS character_snapshots_character ON character_snapshots (character_id, id);
CREATE INDEX IF NOT EXISTS character_snapshots_taken ON character_snapshots (taken_at);
