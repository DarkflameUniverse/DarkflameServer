CREATE TABLE IF NOT EXISTS accounts_rewardcodes (
    account_id INTEGER NOT NULL REFERENCES accounts(id) ON DELETE CASCADE,
    rewardcode INTEGER NOT NULL,
    PRIMARY KEY (account_id, rewardcode)
);
