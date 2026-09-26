/* Server-wide community challenges. See the MySQL migration. */
CREATE TABLE IF NOT EXISTS challenges (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    title TEXT NOT NULL,
    description TEXT NOT NULL DEFAULT '',
    metric_kind INTEGER NOT NULL DEFAULT 0,
    metric INTEGER NOT NULL,
    lot INTEGER NOT NULL DEFAULT 0,
    zones TEXT NOT NULL DEFAULT '',
    target BIGINT NOT NULL,
    starts_at BIGINT NOT NULL,
    ends_at BIGINT NOT NULL,
    include_staff INTEGER NOT NULL DEFAULT 0,
    is_public INTEGER NOT NULL DEFAULT 1,
    reward_coins BIGINT NOT NULL DEFAULT 0,
    reward_items TEXT NOT NULL DEFAULT '[]',
    reward_min BIGINT NOT NULL DEFAULT 1,
    state INTEGER NOT NULL DEFAULT 0,
    milestone INTEGER NOT NULL DEFAULT 0,
    completed_at BIGINT NOT NULL DEFAULT 0,
    rewarded_at BIGINT NOT NULL DEFAULT 0,
    rewarded_count INTEGER NOT NULL DEFAULT 0,
    created_at BIGINT NOT NULL DEFAULT 0,
    created_by TEXT NOT NULL DEFAULT '',
    updated_at BIGINT NOT NULL DEFAULT 0,
    updated_by TEXT NOT NULL DEFAULT ''
);
CREATE INDEX IF NOT EXISTS challenges_state ON challenges (state);
CREATE TABLE IF NOT EXISTS challenge_contributions (
    challenge_id INTEGER NOT NULL,
    character_id BIGINT NOT NULL,
    amount BIGINT NOT NULL DEFAULT 0,
    updated_at BIGINT NOT NULL DEFAULT 0,
    PRIMARY KEY (challenge_id, character_id)
);
CREATE INDEX IF NOT EXISTS challenge_contributions_character ON challenge_contributions (character_id);
CREATE TABLE IF NOT EXISTS challenge_rewards (
    challenge_id INTEGER NOT NULL,
    character_id BIGINT NOT NULL,
    amount BIGINT NOT NULL DEFAULT 0,
    coins BIGINT NOT NULL DEFAULT 0,
    rewarded_at BIGINT NOT NULL DEFAULT 0,
    claimed_at BIGINT NOT NULL DEFAULT 0,
    PRIMARY KEY (challenge_id, character_id)
);
CREATE INDEX IF NOT EXISTS challenge_rewards_character ON challenge_rewards (character_id, claimed_at);
