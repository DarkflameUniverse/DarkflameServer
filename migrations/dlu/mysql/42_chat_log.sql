/* Chat, for moderation and for bots that bridge it to Discord and the like. channel: zone (said in a world),
   whisper, team, web (sent from the dashboard or a bot). blocked: the chat filter stopped it, so nobody saw it. */
CREATE TABLE IF NOT EXISTS chat_log (
    id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
    time BIGINT NOT NULL,
    channel VARCHAR(16) NOT NULL,
    sender_id BIGINT NOT NULL DEFAULT 0,
    sender_name VARCHAR(64) NOT NULL DEFAULT '',
    account_id INT UNSIGNED NOT NULL DEFAULT 0,
    recipient_id BIGINT NOT NULL DEFAULT 0,
    recipient_name VARCHAR(64) NOT NULL DEFAULT '',
    zone_id INT UNSIGNED NOT NULL DEFAULT 0,
    instance_id INT UNSIGNED NOT NULL DEFAULT 0,
    clone_id INT UNSIGNED NOT NULL DEFAULT 0,
    message TEXT NOT NULL,
    blocked TINYINT NOT NULL DEFAULT 0,
    INDEX chat_log_time (time),
    INDEX chat_log_sender (sender_id),
    INDEX chat_log_account (account_id)
);
