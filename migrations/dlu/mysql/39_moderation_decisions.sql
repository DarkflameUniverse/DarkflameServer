/* Moderators' decisions on name requests and pet names, with the reason, so players can see why a name was turned down.
   subject_id is the character (kind 'name') or the pet (kind 'pet_name'); subject is the name that was asked for. */
CREATE TABLE IF NOT EXISTS moderation_decisions (
    id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
    kind VARCHAR(16) NOT NULL,
    subject_id BIGINT NOT NULL,
    subject VARCHAR(64) NOT NULL DEFAULT '',
    approved TINYINT NOT NULL DEFAULT 0,
    reason VARCHAR(255) NOT NULL DEFAULT '',
    decided_at BIGINT NOT NULL DEFAULT 0,
    INDEX moderation_decisions_subject (kind, subject_id)
);
