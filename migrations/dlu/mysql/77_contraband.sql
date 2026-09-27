/* Contraband: items staff don't want players to have, edited on the dashboard's Contraband page. action: 0 = flag
   (an economy flag of kind 5 when a character has one), 1 = flag and remove the item. */
CREATE TABLE IF NOT EXISTS contraband_items (
    lot INT NOT NULL PRIMARY KEY,
    reason TEXT NOT NULL,
    action TINYINT NOT NULL DEFAULT 0,
    added_by VARCHAR(64) NOT NULL DEFAULT '',
    added_at BIGINT NOT NULL DEFAULT 0
);
