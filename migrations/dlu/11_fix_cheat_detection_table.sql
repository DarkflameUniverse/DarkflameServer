DROP TABLE IF EXISTS `player_cheat_detections`;
CREATE TABLE IF NOT EXISTS player_cheat_detections (
    id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
    account_id INTEGER REFERENCES accounts(id),
    name TEXT NOT NULL,
    violation_msg TEXT NOT NULL,
    violation_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    violation_system_address TEXT NOT NULL
);
