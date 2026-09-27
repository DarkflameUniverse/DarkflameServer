/* Stale save guard: see the MySQL migration. */
ALTER TABLE charxml ADD COLUMN save_generation INTEGER NOT NULL DEFAULT 0;
