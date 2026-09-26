/* Every change of a setting's web value, written when it is saved on the dashboard: before and after, who and when.
   file_value: what the files or environment set then. Secrets (secret = 1) keep no values. removed: the setting was
   forgotten entirely. revert_of: the change this one undid. */
CREATE TABLE IF NOT EXISTS server_config_history (
    id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
    file VARCHAR(64) NOT NULL,
    name VARCHAR(128) NOT NULL,
    old_value TEXT NULL DEFAULT NULL,
    old_web_wins TINYINT NOT NULL DEFAULT 0,
    new_value TEXT NULL DEFAULT NULL,
    new_web_wins TINYINT NOT NULL DEFAULT 0,
    file_value TEXT NULL DEFAULT NULL,
    secret TINYINT NOT NULL DEFAULT 0,
    removed TINYINT NOT NULL DEFAULT 0,
    revert_of BIGINT UNSIGNED NOT NULL DEFAULT 0,
    changed_at BIGINT NOT NULL,
    account_id INT UNSIGNED NOT NULL DEFAULT 0,
    changed_by VARCHAR(64) NOT NULL DEFAULT '',
    INDEX server_config_history_setting (file, name, id)
);
