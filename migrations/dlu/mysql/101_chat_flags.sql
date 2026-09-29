/* Chat messages staff flagged for review (the dashboard's Chat Flags page). status: open, actioned or dismissed.
   character_id/account_id: who the flag is about. messages: JSON copy of the flagged messages and the chat around them,
   kept after chat_log prunes them. player_report_id: a player report the flag goes with (0: none).
   chat_flag_messages: which chat_log rows a flag covers. chat_flag_events: who did what with a flag. */
CREATE TABLE IF NOT EXISTS chat_flags (
    id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
    created_at BIGINT NOT NULL,
    created_by_id INT UNSIGNED NOT NULL DEFAULT 0,
    created_by VARCHAR(64) NOT NULL DEFAULT '',
    status VARCHAR(16) NOT NULL DEFAULT 'open',
    channel VARCHAR(16) NOT NULL,
    character_id BIGINT NOT NULL DEFAULT 0,
    character_name VARCHAR(64) NOT NULL DEFAULT '',
    account_id INT UNSIGNED NOT NULL DEFAULT 0,
    first_message_id BIGINT UNSIGNED NOT NULL DEFAULT 0,
    last_message_id BIGINT UNSIGNED NOT NULL DEFAULT 0,
    first_time BIGINT NOT NULL DEFAULT 0,
    last_time BIGINT NOT NULL DEFAULT 0,
    excerpt VARCHAR(512) NOT NULL DEFAULT '',
    note TEXT NOT NULL,
    messages MEDIUMTEXT NOT NULL,
    player_report_id BIGINT UNSIGNED NOT NULL DEFAULT 0,
    updated_at BIGINT NOT NULL DEFAULT 0,
    updated_by VARCHAR(64) NOT NULL DEFAULT '',
    INDEX chat_flags_status (status),
    INDEX chat_flags_character (character_id),
    INDEX chat_flags_account (account_id)
);

CREATE TABLE IF NOT EXISTS chat_flag_messages (
    flag_id BIGINT UNSIGNED NOT NULL,
    message_id BIGINT UNSIGNED NOT NULL,
    PRIMARY KEY (flag_id, message_id),
    INDEX chat_flag_messages_message (message_id)
);

CREATE TABLE IF NOT EXISTS chat_flag_events (
    id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
    flag_id BIGINT UNSIGNED NOT NULL,
    time BIGINT NOT NULL,
    account_id INT UNSIGNED NOT NULL DEFAULT 0,
    actor VARCHAR(64) NOT NULL DEFAULT '',
    action VARCHAR(16) NOT NULL,
    detail TEXT NOT NULL,
    INDEX chat_flag_events_flag (flag_id)
);
