/* What a dashboard action was about, so an account's history is a lookup instead of a text search. 0: nothing. */
ALTER TABLE audit_log ADD COLUMN target_account_id INT UNSIGNED NOT NULL DEFAULT 0;
ALTER TABLE audit_log ADD COLUMN target_character_id BIGINT NOT NULL DEFAULT 0;
CREATE INDEX audit_log_target_account ON audit_log (target_account_id);
