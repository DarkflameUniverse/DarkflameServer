/* slash_commands.target_rule and follows_permission: see the MySQL migration. */
ALTER TABLE slash_commands ADD COLUMN target_rule TEXT NOT NULL DEFAULT '';
ALTER TABLE slash_commands ADD COLUMN follows_permission INTEGER NOT NULL DEFAULT 0;
