DROP TABLE IF EXISTS `player_cheat_detections`;
CREATE TABLE IF NOT EXISTS player_cheat_detections (
    id BIGINT NOT NULL AUTO_INCREMENT PRIMARY KEY,
    account_id INT REFERENCES accounts(id),
    name TEXT NOT NULL,
    violation_msg TEXT NOT NULL,
    violation_time TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP(),
    violation_system_address TEXT NOT NULL
);
