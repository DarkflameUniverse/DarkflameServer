/* chat_log guild_id, team_id and filtered. See the MySQL migration. */
ALTER TABLE chat_log ADD COLUMN guild_id BIGINT NOT NULL DEFAULT 0;
ALTER TABLE chat_log ADD COLUMN team_id BIGINT NOT NULL DEFAULT 0;
ALTER TABLE chat_log ADD COLUMN filtered INTEGER NOT NULL DEFAULT 0;
UPDATE chat_log SET filtered = blocked;
CREATE INDEX IF NOT EXISTS chat_log_guild ON chat_log (guild_id);
CREATE INDEX IF NOT EXISTS chat_log_team ON chat_log (team_id);
