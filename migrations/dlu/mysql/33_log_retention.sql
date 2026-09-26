/* Lets old command log rows be pruned by age. Rows written before this have time 0 and are kept. */
ALTER TABLE command_log ADD COLUMN time BIGINT NOT NULL DEFAULT 0;
CREATE INDEX command_log_time ON command_log (time);
CREATE INDEX activity_log_time ON activity_log (time);
