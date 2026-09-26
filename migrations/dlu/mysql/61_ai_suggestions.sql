/* The dashboard's staff-side AI moderator helper: every suggestion asked for, with who asked, the model and tokens, kept with the item it is about. Drafts for staff only. */
CREATE TABLE IF NOT EXISTS ai_suggestions (
    id BIGINT NOT NULL AUTO_INCREMENT PRIMARY KEY,
    kind VARCHAR(32) NOT NULL,
    item_id BIGINT NOT NULL,
    fingerprint CHAR(64) NOT NULL,
    requested_by_id INT UNSIGNED NOT NULL DEFAULT 0,
    requested_by VARCHAR(64) NOT NULL DEFAULT '',
    created_at BIGINT NOT NULL,
    model VARCHAR(100) NOT NULL DEFAULT '',
    input_tokens INT UNSIGNED NOT NULL DEFAULT 0,
    output_tokens INT UNSIGNED NOT NULL DEFAULT 0,
    status TINYINT NOT NULL DEFAULT 0,
    suggestion TEXT NULL,
    error TEXT NULL,
    context MEDIUMTEXT NULL,
    INDEX ai_suggestions_item (kind, item_id, id),
    INDEX ai_suggestions_time (created_at)
);
