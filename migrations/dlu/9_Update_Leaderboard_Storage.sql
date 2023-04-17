ALTER TABLE leaderboard 
    ADD COLUMN hitPercentage FLOAT NOT NULL DEFAULT 0,
    ADD COLUMN streak INT NOT NULL DEFAULT 0,
    ADD COLUMN bestLapTime FLOAT NOT NULL DEFAULT 0,
    ADD COLUMN numWins INT NOT NULL DEFAULT 0,
    MODIFY time FLOAT NOT NULL DEFAULT 0;

ALTER TABLE leaderboard CHANGE time bestTime float;

UPDATE leaderboard SET streak = bestTime where game_id = 1864;
