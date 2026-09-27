/* The brick by brick model each character was building when their client last sent SetBBBAutosave, and the model items
   that were in their BBB inventory at that time. Rebuilt into models when a build ends without a save (see
   docs/BuildWorkflow.md). */
CREATE TABLE IF NOT EXISTS bbb_autosave (
    character_id BIGINT NOT NULL PRIMARY KEY,
    lxfml MEDIUMBLOB NOT NULL,
    source_items TEXT NOT NULL,
    updated_at BIGINT NOT NULL DEFAULT 0
);
