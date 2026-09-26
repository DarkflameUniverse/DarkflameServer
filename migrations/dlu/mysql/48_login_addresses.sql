/* The network addresses accounts log in to the game from, written by the auth server at each successful login while
   log_login_addresses is on, so staff can see accounts that share a connection. One row per account and address;
   rows not seen for log_login_address_days are deleted by the log pruning task. */
CREATE TABLE IF NOT EXISTS account_login_addresses (
    account_id INT UNSIGNED NOT NULL,
    address VARCHAR(64) NOT NULL,
    first_seen BIGINT NOT NULL,
    last_seen BIGINT NOT NULL,
    logins INT UNSIGNED NOT NULL DEFAULT 1,
    PRIMARY KEY (account_id, address),
    INDEX account_login_addresses_address (address)
);
