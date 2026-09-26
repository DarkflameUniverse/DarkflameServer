/* Strikes against accounts, given when staff reject a name, pet name or property or remove a leaderboard score and
   decide it deserves one (or by hand). source: MANUAL, NAME, PET_NAME, PROPERTY, LEADERBOARD. A revoked strike
   (revoked_at > 0) stays on record but no longer counts. */
CREATE TABLE IF NOT EXISTS account_strikes (
    id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
    account_id INT UNSIGNED NOT NULL,
    character_id BIGINT NOT NULL DEFAULT 0,
    source VARCHAR(16) NOT NULL,
    subject VARCHAR(128) NOT NULL DEFAULT '',
    reason TEXT NOT NULL,
    given_by_id INT UNSIGNED NOT NULL DEFAULT 0,
    given_by VARCHAR(64) NOT NULL DEFAULT '',
    created_at BIGINT NOT NULL,
    revoked_at BIGINT NOT NULL DEFAULT 0,
    revoked_by VARCHAR(64) NOT NULL DEFAULT '',
    revoke_reason TEXT NOT NULL,
    INDEX account_strikes_account (account_id)
);
