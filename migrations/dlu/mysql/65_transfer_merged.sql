/* economy_transfers.merged: 1 when new_item_id is a stack the receiver already had (the items joined it), 0 when the
   hop made a new object. NULL for rows written before this was recorded. method 4 (INVENTORY_MOVE) is a move between
   a character's own inventories, which also gives the item a new object id. */
ALTER TABLE economy_transfers ADD COLUMN merged TINYINT DEFAULT NULL;
