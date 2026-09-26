/* Columns used by the web dashboard. Names match NexusDashboard. */
ALTER TABLE play_keys ADD COLUMN notes TEXT DEFAULT NULL;
ALTER TABLE bug_reports ADD COLUMN resolved_time DATETIME DEFAULT NULL;
ALTER TABLE bug_reports ADD COLUMN resoleved_by_id INTEGER DEFAULT NULL;
ALTER TABLE bug_reports ADD COLUMN resolution TEXT DEFAULT NULL;
