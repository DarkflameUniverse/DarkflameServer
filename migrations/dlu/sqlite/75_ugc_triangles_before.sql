/* UGC server: a model's triangles before hidden faces were removed. See the MySQL migration. */
ALTER TABLE ugc ADD COLUMN triangle_count_before INTEGER NOT NULL DEFAULT 0;
