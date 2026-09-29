/* Permissions and slash commands granted to (deny = 0) or taken from (deny = 1) one account or character, on top of
   what its GM level allows. target_type: 'account' (target_id = accounts.id) or 'character' (target_id = charinfo.id).
   kind: 'permission' (name = a dashboard permission key), 'command' (name = a slash command's settings name),
   'permission_group' (name = a permission category) or 'command_group' (name = a GM level: every command up to it).
   expires_at: 0 for never. Rows are never deleted: removing one sets revoked_at and revoked_by, so the table is also
   the history of grant changes. granted_by_id is the granting account. */
CREATE TABLE IF NOT EXISTS permission_grants (
    id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
    target_type VARCHAR(16) NOT NULL,
    target_id BIGINT NOT NULL,
    kind VARCHAR(24) NOT NULL,
    name VARCHAR(64) NOT NULL,
    deny TINYINT NOT NULL DEFAULT 0,
    expires_at BIGINT NOT NULL DEFAULT 0,
    note VARCHAR(255) NOT NULL DEFAULT '',
    granted_at BIGINT NOT NULL,
    granted_by_id INT UNSIGNED NOT NULL DEFAULT 0,
    granted_by VARCHAR(64) NOT NULL DEFAULT '',
    revoked_at BIGINT NOT NULL DEFAULT 0,
    revoked_by VARCHAR(64) NOT NULL DEFAULT '',
    INDEX permission_grants_target (target_type, target_id)
);
