/* What a dashboard action was about. See the MySQL migration. */
ALTER TABLE audit_log ADD COLUMN target_account_id INTEGER NOT NULL DEFAULT 0;
ALTER TABLE audit_log ADD COLUMN target_character_id BIGINT NOT NULL DEFAULT 0;
CREATE INDEX IF NOT EXISTS audit_log_target_account ON audit_log (target_account_id);
