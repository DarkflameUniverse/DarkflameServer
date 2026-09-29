/* Permissions and slash commands granted to or taken from one account or character. See the MySQL migration. */
CREATE TABLE IF NOT EXISTS permission_grants (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    target_type TEXT NOT NULL,
    target_id BIGINT NOT NULL,
    kind TEXT NOT NULL,
    name TEXT NOT NULL,
    deny INTEGER NOT NULL DEFAULT 0,
    expires_at BIGINT NOT NULL DEFAULT 0,
    note TEXT NOT NULL DEFAULT '',
    granted_at BIGINT NOT NULL,
    granted_by_id INTEGER NOT NULL DEFAULT 0,
    granted_by TEXT NOT NULL DEFAULT '',
    revoked_at BIGINT NOT NULL DEFAULT 0,
    revoked_by TEXT NOT NULL DEFAULT ''
);
CREATE INDEX IF NOT EXISTS permission_grants_target ON permission_grants (target_type, target_id);
