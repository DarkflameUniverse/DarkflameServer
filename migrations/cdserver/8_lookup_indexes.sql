-- The converted CDClient has no indexes, so every lazy lookup by id scans its whole table (ComponentsRegistry: about
-- 51000 rows, 3.5 ms a query). Loading a player with a big inventory did thousands of them on the world's main loop.
CREATE INDEX IF NOT EXISTS idx_ComponentsRegistry_id ON ComponentsRegistry (id);
CREATE INDEX IF NOT EXISTS idx_ItemComponent_id ON ItemComponent (id);
CREATE INDEX IF NOT EXISTS idx_Objects_id ON Objects (id);
CREATE INDEX IF NOT EXISTS idx_LootMatrix_LootMatrixIndex ON LootMatrix (LootMatrixIndex);
CREATE INDEX IF NOT EXISTS idx_LootTable_LootTableIndex ON LootTable (LootTableIndex);
CREATE INDEX IF NOT EXISTS idx_PlayerFlags_id ON PlayerFlags (id);
CREATE INDEX IF NOT EXISTS idx_Animations_animationGroupID ON Animations (animationGroupID, animation_type);
