/* featured_properties: what each slot of the news screen's "Today's Top Properties" panel shows, set on the dashboard.
   template_id: the slot, a PropertyTemplate id (one per small property world the client's news screen knows).
   mode: 0 the world's approved public property with the most reputation, 1 property_id, 2 nothing.
   A slot without a row is mode 0. */
CREATE TABLE IF NOT EXISTS featured_properties (
	template_id INT UNSIGNED NOT NULL PRIMARY KEY,
	mode TINYINT UNSIGNED NOT NULL DEFAULT 0,
	property_id BIGINT NOT NULL DEFAULT 0,
	updated_at BIGINT NOT NULL DEFAULT 0,
	updated_by VARCHAR(64) NOT NULL DEFAULT ''
);
