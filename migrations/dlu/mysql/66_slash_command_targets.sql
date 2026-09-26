/* slash_commands.target_rule: who a command may be used on, by the dashboard's self and rank rules (SlashCommandLevels::eTargetRule):
   '' (acts on nobody else), 'tools', 'items' or 'moderation' (on yourself with self_tools, self_items or self_moderation),
   or 'others' (checked on other players only). Written by the world servers like the rest of the row.
   slash_commands.follows_permission: 1 when the command uses its dashboard_permission's level (written by servers that know
   this). Rows from before are 0, which is how a world tells that a server is upgrading and keeps the level each paired command
   had then as an override (command_level_<name>, updated_by '[upgrade]'). */
ALTER TABLE slash_commands ADD COLUMN target_rule VARCHAR(16) NOT NULL DEFAULT '';
ALTER TABLE slash_commands ADD COLUMN follows_permission TINYINT NOT NULL DEFAULT 0;
