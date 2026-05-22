CREATE TABLE IF NOT EXISTS property_reputation_contribution (
    property_id BIGINT NOT NULL,
    player_id BIGINT NOT NULL,
    contribution_date DATE NOT NULL,
    reputation_gained INT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (property_id, player_id, contribution_date)
);
