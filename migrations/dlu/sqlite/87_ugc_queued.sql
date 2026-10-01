/* ugc.queued: see the MySQL migration. */
ALTER TABLE ugc ADD COLUMN queued INTEGER NOT NULL DEFAULT 0;
