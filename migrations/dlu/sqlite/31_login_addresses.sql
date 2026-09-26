/* Where accounts log in to the game from. See the MySQL migration. */
CREATE TABLE IF NOT EXISTS account_login_addresses (
    account_id INTEGER NOT NULL,
    address TEXT NOT NULL,
    first_seen BIGINT NOT NULL,
    last_seen BIGINT NOT NULL,
    logins INTEGER NOT NULL DEFAULT 1,
    PRIMARY KEY (account_id, address)
);
CREATE INDEX IF NOT EXISTS account_login_addresses_address ON account_login_addresses (address);
