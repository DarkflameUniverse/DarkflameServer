/* Guilds, owned by the chat server. See the MySQL migration. */
CREATE TABLE IF NOT EXISTS guilds (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL COLLATE NOCASE UNIQUE,
    name_status INTEGER NOT NULL DEFAULT 1,
    founder_id BIGINT NOT NULL DEFAULT 0,
    created_at BIGINT NOT NULL
);

CREATE TABLE IF NOT EXISTS guild_members (
    character_id BIGINT NOT NULL PRIMARY KEY,
    guild_id BIGINT NOT NULL,
    guild_rank INTEGER NOT NULL,
    joined_at BIGINT NOT NULL
);
CREATE INDEX IF NOT EXISTS guild_members_guild ON guild_members (guild_id);

CREATE TABLE IF NOT EXISTS guild_invites (
    character_id BIGINT NOT NULL PRIMARY KEY,
    guild_id BIGINT NOT NULL,
    inviter_id BIGINT NOT NULL,
    created_at BIGINT NOT NULL
);
CREATE INDEX IF NOT EXISTS guild_invites_guild ON guild_invites (guild_id);

CREATE TABLE IF NOT EXISTS guild_events (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    guild_id BIGINT NOT NULL,
    time BIGINT NOT NULL,
    kind TEXT NOT NULL,
    character_id BIGINT NOT NULL DEFAULT 0,
    character_name TEXT NOT NULL DEFAULT '',
    actor TEXT NOT NULL DEFAULT '',
    detail TEXT NOT NULL DEFAULT ''
);
CREATE INDEX IF NOT EXISTS guild_events_guild ON guild_events (guild_id, id);
