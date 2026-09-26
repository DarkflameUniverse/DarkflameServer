/* vanity_events.file_switches: see the MySQL migration. */
ALTER TABLE vanity_events ADD COLUMN file_switches TEXT NOT NULL DEFAULT '';
