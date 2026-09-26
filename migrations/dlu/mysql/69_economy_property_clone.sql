/* Property worlds run one instance per property, all on the same zone: map events and player statistics now also
   record the instance's clone id (the property owner's charinfo.prop_clone_id, as the world knows it), so one
   property's data can be told apart from another's. 0 on every other zone, and on the rows written before this, which
   the reports show as "unknown property" on property zones.
   The map event kinds grow too: 8 = powerup dropped (lot = the powerup), 9 = powerup picked up. */
ALTER TABLE map_events_daily ADD COLUMN clone_id INT UNSIGNED NOT NULL DEFAULT 0,
    DROP PRIMARY KEY, ADD PRIMARY KEY (zone, kind, day, clone_id, lot, cell_x, cell_z);
ALTER TABLE player_stats_daily ADD COLUMN clone_id INT UNSIGNED NOT NULL DEFAULT 0,
    DROP PRIMARY KEY, ADD PRIMARY KEY (day, zone, clone_id, stat, gm);
