/* The in-game slash commands as the world servers registered them, so the dashboard can list them and change who may
   use them. The levels themselves are settings (command_level_<name> in server_config). aliases is comma separated. */
CREATE TABLE IF NOT EXISTS slash_commands (
    name VARCHAR(64) NOT NULL PRIMARY KEY,
    aliases TEXT NOT NULL,
    help TEXT NOT NULL,
    info TEXT NOT NULL,
    default_level TINYINT NOT NULL,
    min_level TINYINT NOT NULL DEFAULT 0,
    fixed TINYINT NOT NULL DEFAULT 0,
    client_handled TINYINT NOT NULL DEFAULT 0,
    note TEXT NOT NULL,
    dashboard_permission VARCHAR(64) NOT NULL DEFAULT ''
);
