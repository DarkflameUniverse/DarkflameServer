/* Earlier versions of characters' XML (zlib-compressed), taken before dashboard edits and daily for changed characters. */
CREATE TABLE IF NOT EXISTS character_snapshots (
    id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
    character_id BIGINT NOT NULL,
    taken_at BIGINT NOT NULL,
    reason VARCHAR(100) NOT NULL DEFAULT '',
    actor VARCHAR(64) NOT NULL DEFAULT '',
    size INT UNSIGNED NOT NULL DEFAULT 0,
    hash CHAR(64) NOT NULL DEFAULT '',
    xml LONGBLOB NOT NULL,
    INDEX character_snapshots_character (character_id, id),
    INDEX character_snapshots_taken (taken_at)
);
