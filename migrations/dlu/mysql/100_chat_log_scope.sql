/* Where a chat message went, for the guild and team chat histories: guild_id (channel guild) is guilds.id, team_id
   (channel team) is the chat server's team, unique across chat server restarts. filtered: the chat filter found words it
   doesn't allow. Zone chat that is filtered is also blocked (nobody saw it); whispers, team and guild chat are delivered
   either way, so filtered only records what the filter thought of them. */
ALTER TABLE chat_log ADD COLUMN guild_id BIGINT NOT NULL DEFAULT 0;
ALTER TABLE chat_log ADD COLUMN team_id BIGINT NOT NULL DEFAULT 0;
ALTER TABLE chat_log ADD COLUMN filtered TINYINT NOT NULL DEFAULT 0;
UPDATE chat_log SET filtered = blocked;
CREATE INDEX chat_log_guild ON chat_log (guild_id);
CREATE INDEX chat_log_team ON chat_log (team_id);
