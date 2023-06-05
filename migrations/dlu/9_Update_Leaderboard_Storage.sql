ALTER TABLE leaderboard 
    ADD COLUMN tertiaryScore FLOAT NOT NULL DEFAULT 0,
    ADD COLUMN numWins INT NOT NULL DEFAULT 0,
    ADD COLUMN timesPlayed INT NOT NULL DEFAULT 1,
    MODIFY time INT NOT NULL DEFAULT 0;

/* Can only ALTER one column at a time... */
ALTER TABLE leaderboard 
	CHANGE last_played last_played TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP() ON UPDATE CURRENT_TIMESTAMP();
ALTER TABLE leaderboard CHANGE score primaryScore FLOAT NOT NULL DEFAULT 0;
ALTER TABLE leaderboard CHANGE time secondaryScore FLOAT NOT NULL DEFAULT 0 AFTER primaryScore;

/* A bit messy, but better than going through a bunch of code fixes all to be run once. */
UPDATE leaderboard SET
	primaryScore = secondaryScore,
	secondaryScore = 0 WHERE game_id IN (1, 44, 46, 47, 48, 49, 53, 103, 104, 108, 1901);
