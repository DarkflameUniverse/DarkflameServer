/* One row a minute from the dashboard: players online, running worlds, whether auth and chat were up, memory in use. */
CREATE TABLE IF NOT EXISTS server_health (
    time BIGINT NOT NULL PRIMARY KEY,
    players INT UNSIGNED NOT NULL DEFAULT 0,
    worlds INT UNSIGNED NOT NULL DEFAULT 0,
    auth_online TINYINT NOT NULL DEFAULT 0,
    chat_online TINYINT NOT NULL DEFAULT 0,
    memory_kb BIGINT UNSIGNED NOT NULL DEFAULT 0
);
