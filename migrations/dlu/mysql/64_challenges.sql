/* Server-wide community challenges: reach target of a metric between starts_at and ends_at. metric_kind 0 = a player
   statistic (StatisticID), 1 = a map event (IEconomyLedger::eMapEvent, optionally only lot). zones: '' for every zone.
   state: 0 open, 1 completed, 2 expired (ended short of the target), 3 cancelled. milestone: the last percentage
   announced. reward_items: JSON [{lot, count}] mailed to each character that contributed at least reward_min. */
CREATE TABLE IF NOT EXISTS challenges (
    id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
    title VARCHAR(100) NOT NULL,
    description TEXT NOT NULL,
    metric_kind TINYINT NOT NULL DEFAULT 0,
    metric INT UNSIGNED NOT NULL,
    lot INT NOT NULL DEFAULT 0,
    zones TEXT NOT NULL,
    target BIGINT NOT NULL,
    starts_at BIGINT NOT NULL,
    ends_at BIGINT NOT NULL,
    include_staff TINYINT NOT NULL DEFAULT 0,
    is_public TINYINT NOT NULL DEFAULT 1,
    reward_coins BIGINT NOT NULL DEFAULT 0,
    reward_items TEXT NOT NULL,
    reward_min BIGINT NOT NULL DEFAULT 1,
    state TINYINT NOT NULL DEFAULT 0,
    milestone TINYINT UNSIGNED NOT NULL DEFAULT 0,
    completed_at BIGINT NOT NULL DEFAULT 0,
    rewarded_at BIGINT NOT NULL DEFAULT 0,
    rewarded_count INT UNSIGNED NOT NULL DEFAULT 0,
    created_at BIGINT NOT NULL DEFAULT 0,
    created_by VARCHAR(64) NOT NULL DEFAULT '',
    updated_at BIGINT NOT NULL DEFAULT 0,
    updated_by VARCHAR(64) NOT NULL DEFAULT '',
    INDEX challenges_state (state)
);
/* How much each character added to a challenge, counted by the world servers where the game records it. */
CREATE TABLE IF NOT EXISTS challenge_contributions (
    challenge_id BIGINT UNSIGNED NOT NULL,
    character_id BIGINT NOT NULL,
    amount BIGINT NOT NULL DEFAULT 0,
    updated_at BIGINT NOT NULL DEFAULT 0,
    PRIMARY KEY (challenge_id, character_id),
    INDEX challenge_contributions_character (character_id)
);
/* Rewards given for a completed challenge: one row per character (items are mailed; coins wait until claimed_at is
   set by the world the character plays in). */
CREATE TABLE IF NOT EXISTS challenge_rewards (
    challenge_id BIGINT UNSIGNED NOT NULL,
    character_id BIGINT NOT NULL,
    amount BIGINT NOT NULL DEFAULT 0,
    coins BIGINT NOT NULL DEFAULT 0,
    rewarded_at BIGINT NOT NULL DEFAULT 0,
    claimed_at BIGINT NOT NULL DEFAULT 0,
    PRIMARY KEY (challenge_id, character_id),
    INDEX challenge_rewards_character (character_id, claimed_at)
);
