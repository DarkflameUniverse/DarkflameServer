/* Economy ledger for dashboard reports. Flows are aggregated per day (days since the Unix epoch, UTC)
   so storage grows with active players and item types, not with the number of events. */
CREATE TABLE IF NOT EXISTS economy_currency_daily (
    day INT NOT NULL,
    character_id BIGINT NOT NULL,
    source INT NOT NULL,
    gained BIGINT NOT NULL DEFAULT 0,
    spent BIGINT NOT NULL DEFAULT 0,
    PRIMARY KEY (day, character_id, source),
    INDEX economy_currency_character (character_id, day)
);
CREATE TABLE IF NOT EXISTS economy_uscore_daily (
    day INT NOT NULL,
    character_id BIGINT NOT NULL,
    source INT NOT NULL,
    gained BIGINT NOT NULL DEFAULT 0,
    lost BIGINT NOT NULL DEFAULT 0,
    PRIMARY KEY (day, character_id, source)
);
CREATE TABLE IF NOT EXISTS economy_items_daily (
    day INT NOT NULL,
    lot INT NOT NULL,
    source INT NOT NULL,
    gm TINYINT NOT NULL DEFAULT 0,
    created BIGINT NOT NULL DEFAULT 0,
    destroyed BIGINT NOT NULL DEFAULT 0,
    PRIMARY KEY (day, lot, source, gm),
    INDEX economy_items_lot (lot, day)
);
/* Player-to-player item and coin transfers. item_id is the object that left the sender, new_item_id the item or
   stack that received it (the same id when a unique item or unmerged stack kept its identity, otherwise the stack
   it merged into or split off as). */
CREATE TABLE IF NOT EXISTS economy_transfers (
    id BIGINT NOT NULL AUTO_INCREMENT PRIMARY KEY,
    time BIGINT NOT NULL,
    method TINYINT NOT NULL,
    item_id BIGINT NOT NULL DEFAULT 0,
    new_item_id BIGINT NOT NULL DEFAULT 0,
    lot INT NOT NULL DEFAULT 0,
    count INT NOT NULL DEFAULT 0,
    coins BIGINT NOT NULL DEFAULT 0,
    from_character BIGINT NOT NULL DEFAULT 0,
    to_character BIGINT NOT NULL DEFAULT 0,
    zone INT NOT NULL DEFAULT 0,
    INDEX economy_transfers_item (item_id),
    INDEX economy_transfers_new_item (new_item_id),
    INDEX economy_transfers_from (from_character, time),
    INDEX economy_transfers_to (to_character, time)
);
/* Where things happen in the world, for heatmaps: enemy kills and loot drops per day, zone, LOT and 4x4 unit cell.
   kind: 1 = enemy killed by a player, 2 = item dropped, 3 = coins dropped (lot 0, quantity = coins). */
CREATE TABLE IF NOT EXISTS map_events_daily (
    day INT NOT NULL,
    zone INT NOT NULL,
    kind TINYINT NOT NULL,
    lot INT NOT NULL,
    cell_x INT NOT NULL,
    cell_z INT NOT NULL,
    events BIGINT NOT NULL DEFAULT 0,
    quantity BIGINT NOT NULL DEFAULT 0,
    PRIMARY KEY (zone, kind, day, lot, cell_x, cell_z)
);
ALTER TABLE mail ADD COLUMN attachment_config TEXT NULL;
