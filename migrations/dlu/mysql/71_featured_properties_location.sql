/* featured_properties.zone_id: the property world a "Today's Top Properties" slot shows a property of (any world with a
   property entrance); 0, as on every row from before, is the slot's own world.
   featured_properties_settings: one row (id 1). full_auto 1: the slots' rows are ignored and the panel shows the four
   approved public properties with the most reputation across every property world. No row: full_auto 0. */
ALTER TABLE featured_properties ADD COLUMN zone_id INT UNSIGNED NOT NULL DEFAULT 0;
CREATE TABLE IF NOT EXISTS featured_properties_settings (
	id TINYINT UNSIGNED NOT NULL PRIMARY KEY,
	full_auto TINYINT NOT NULL DEFAULT 0,
	updated_at BIGINT NOT NULL DEFAULT 0,
	updated_by VARCHAR(64) NOT NULL DEFAULT ''
);
