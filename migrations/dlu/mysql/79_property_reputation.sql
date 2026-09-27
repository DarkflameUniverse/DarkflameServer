/* Property reputation: what each visitor account gave each property per UTC day (days since the Unix epoch), for the
   per-visitor and per-property daily caps and to count repeat visitors (see dCommon/PropertyReputationRules.h).
   properties.reputation holds the total. */
CREATE TABLE IF NOT EXISTS property_reputation_visits (
    property_id BIGINT NOT NULL,
    account_id INT NOT NULL,
    day INT NOT NULL,
    points BIGINT NOT NULL DEFAULT 0,
    seconds BIGINT NOT NULL DEFAULT 0,
    PRIMARY KEY (property_id, account_id, day),
    INDEX property_reputation_visits_day (property_id, day)
);
