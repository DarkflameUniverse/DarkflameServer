/* Server health samples. See the MySQL migration. */
CREATE TABLE IF NOT EXISTS server_health (
    time BIGINT NOT NULL PRIMARY KEY,
    players INTEGER NOT NULL DEFAULT 0,
    worlds INTEGER NOT NULL DEFAULT 0,
    auth_online INTEGER NOT NULL DEFAULT 0,
    chat_online INTEGER NOT NULL DEFAULT 0,
    memory_kb BIGINT NOT NULL DEFAULT 0
);
