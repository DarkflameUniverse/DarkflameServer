/* Economy ledger for dashboard reports. See the MySQL migration for details. */
CREATE TABLE IF NOT EXISTS economy_currency_daily (
    day INTEGER NOT NULL,
    character_id BIGINT NOT NULL,
    source INTEGER NOT NULL,
    gained BIGINT NOT NULL DEFAULT 0,
    spent BIGINT NOT NULL DEFAULT 0,
    PRIMARY KEY (day, character_id, source)
);
CREATE INDEX IF NOT EXISTS economy_currency_character ON economy_currency_daily (character_id, day);
CREATE TABLE IF NOT EXISTS economy_uscore_daily (
    day INTEGER NOT NULL,
    character_id BIGINT NOT NULL,
    source INTEGER NOT NULL,
    gained BIGINT NOT NULL DEFAULT 0,
    lost BIGINT NOT NULL DEFAULT 0,
    PRIMARY KEY (day, character_id, source)
);
CREATE TABLE IF NOT EXISTS economy_items_daily (
    day INTEGER NOT NULL,
    lot INTEGER NOT NULL,
    source INTEGER NOT NULL,
    gm INTEGER NOT NULL DEFAULT 0,
    created BIGINT NOT NULL DEFAULT 0,
    destroyed BIGINT NOT NULL DEFAULT 0,
    PRIMARY KEY (day, lot, source, gm)
);
CREATE INDEX IF NOT EXISTS economy_items_lot ON economy_items_daily (lot, day);
CREATE TABLE IF NOT EXISTS economy_transfers (
    id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
    time BIGINT NOT NULL,
    method INTEGER NOT NULL,
    item_id BIGINT NOT NULL DEFAULT 0,
    new_item_id BIGINT NOT NULL DEFAULT 0,
    lot INTEGER NOT NULL DEFAULT 0,
    count INTEGER NOT NULL DEFAULT 0,
    coins BIGINT NOT NULL DEFAULT 0,
    from_character BIGINT NOT NULL DEFAULT 0,
    to_character BIGINT NOT NULL DEFAULT 0,
    zone INTEGER NOT NULL DEFAULT 0
);
CREATE INDEX IF NOT EXISTS economy_transfers_item ON economy_transfers (item_id);
CREATE INDEX IF NOT EXISTS economy_transfers_new_item ON economy_transfers (new_item_id);
CREATE INDEX IF NOT EXISTS economy_transfers_from ON economy_transfers (from_character, time);
CREATE INDEX IF NOT EXISTS economy_transfers_to ON economy_transfers (to_character, time);
CREATE TABLE IF NOT EXISTS map_events_daily (
    day INTEGER NOT NULL,
    zone INTEGER NOT NULL,
    kind INTEGER NOT NULL,
    lot INTEGER NOT NULL,
    cell_x INTEGER NOT NULL,
    cell_z INTEGER NOT NULL,
    events BIGINT NOT NULL DEFAULT 0,
    quantity BIGINT NOT NULL DEFAULT 0,
    PRIMARY KEY (zone, kind, day, lot, cell_x, cell_z)
);
ALTER TABLE mail ADD COLUMN attachment_config TEXT DEFAULT NULL;
