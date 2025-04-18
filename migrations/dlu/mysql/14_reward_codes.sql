CREATE TABLE IF NOT EXISTS accounts_rewardcodes (
    account_id INT NOT NULL REFERENCES accounts(id) ON DELETE CASCADE,
    rewardcode INT NOT NULL,
    PRIMARY KEY (account_id, rewardcode)
);
