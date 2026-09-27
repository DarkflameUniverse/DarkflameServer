/* UGC server: the bricks and triangles it counted when it made a model. See the MySQL migration. */
ALTER TABLE ugc ADD COLUMN brick_count INTEGER NOT NULL DEFAULT 0;
ALTER TABLE ugc ADD COLUMN triangle_count INTEGER NOT NULL DEFAULT 0;
