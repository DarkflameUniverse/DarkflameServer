/* Property rent: a price (coins, 0: free) and period (days, 0: the template's) per property world set on the
   dashboard, overriding PropertyTemplate's. properties.rent_due (unused until now) is when the next rent is due. */
CREATE TABLE IF NOT EXISTS property_rent_rates (
    map_id INT NOT NULL PRIMARY KEY,
    price BIGINT NOT NULL DEFAULT 0,
    period_days INT NOT NULL DEFAULT 0,
    updated_by VARCHAR(64) NOT NULL DEFAULT '',
    updated_at BIGINT NOT NULL DEFAULT 0
);
