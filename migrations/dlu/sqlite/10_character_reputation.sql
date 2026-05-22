CREATE TABLE IF NOT EXISTS character_reputation (
    character_id BIGINT NOT NULL PRIMARY KEY,
    reputation BIGINT NOT NULL DEFAULT 0
);
