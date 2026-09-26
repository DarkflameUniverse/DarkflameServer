/* The in-game slash commands as the world servers registered them. See the MySQL migration. */
CREATE TABLE IF NOT EXISTS slash_commands (
    name TEXT NOT NULL PRIMARY KEY,
    aliases TEXT NOT NULL DEFAULT '',
    help TEXT NOT NULL DEFAULT '',
    info TEXT NOT NULL DEFAULT '',
    default_level INTEGER NOT NULL,
    min_level INTEGER NOT NULL DEFAULT 0,
    fixed INTEGER NOT NULL DEFAULT 0,
    client_handled INTEGER NOT NULL DEFAULT 0,
    note TEXT NOT NULL DEFAULT '',
    dashboard_permission TEXT NOT NULL DEFAULT ''
);
