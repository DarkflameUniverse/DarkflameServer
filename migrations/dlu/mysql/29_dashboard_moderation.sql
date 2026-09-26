/* Columns used by the web dashboard. Names match NexusDashboard so databases it already migrated keep their data,
   statements for columns that already exist fail individually and are skipped. */
ALTER TABLE play_keys ADD COLUMN notes TEXT NULL;
ALTER TABLE bug_reports ADD COLUMN resolved_time TIMESTAMP NULL DEFAULT NULL;
ALTER TABLE bug_reports ADD COLUMN resoleved_by_id INT NULL;
ALTER TABLE bug_reports ADD COLUMN resolution TEXT NULL;
