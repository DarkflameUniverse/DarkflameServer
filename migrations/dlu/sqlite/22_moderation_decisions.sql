/* Moderators' decisions on name requests and pet names. See the MySQL migration. */
CREATE TABLE IF NOT EXISTS moderation_decisions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    kind TEXT NOT NULL,
    subject_id BIGINT NOT NULL,
    subject TEXT NOT NULL DEFAULT '',
    approved INTEGER NOT NULL DEFAULT 0,
    reason TEXT NOT NULL DEFAULT '',
    decided_at BIGINT NOT NULL DEFAULT 0
);
CREATE INDEX IF NOT EXISTS moderation_decisions_subject ON moderation_decisions (kind, subject_id);
