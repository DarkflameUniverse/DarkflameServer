/* Guilds (docs/Guilds.md), owned by the chat server. The leader is the member with guild_rank 1 (1 leader, 2 officer,
   3 veteran, 4 recruit). name_status: 1 = the name waits for moderation, 2 = approved (as pet_names.approved).
   A character is in at most one guild (guild_members) and has at most one pending invite (guild_invites).
   guild_events is each guild's history; it is kept when a guild is disbanded. */
CREATE TABLE IF NOT EXISTS guilds (
    id BIGINT NOT NULL AUTO_INCREMENT PRIMARY KEY,
    name VARCHAR(64) NOT NULL,
    name_status INT NOT NULL DEFAULT 1,
    founder_id BIGINT NOT NULL DEFAULT 0,
    created_at BIGINT NOT NULL,
    UNIQUE INDEX guilds_name (name)
);

CREATE TABLE IF NOT EXISTS guild_members (
    character_id BIGINT NOT NULL PRIMARY KEY,
    guild_id BIGINT NOT NULL,
    guild_rank TINYINT UNSIGNED NOT NULL,
    joined_at BIGINT NOT NULL,
    INDEX guild_members_guild (guild_id)
);

CREATE TABLE IF NOT EXISTS guild_invites (
    character_id BIGINT NOT NULL PRIMARY KEY,
    guild_id BIGINT NOT NULL,
    inviter_id BIGINT NOT NULL,
    created_at BIGINT NOT NULL,
    INDEX guild_invites_guild (guild_id)
);

CREATE TABLE IF NOT EXISTS guild_events (
    id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
    guild_id BIGINT NOT NULL,
    time BIGINT NOT NULL,
    kind VARCHAR(24) NOT NULL,
    character_id BIGINT NOT NULL DEFAULT 0,
    character_name VARCHAR(35) NOT NULL DEFAULT '',
    actor VARCHAR(64) NOT NULL DEFAULT '',
    detail VARCHAR(255) NOT NULL DEFAULT '',
    INDEX guild_events_guild (guild_id, id)
);
