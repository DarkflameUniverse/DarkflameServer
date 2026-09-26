/* challenges.end_announced_at: see the MySQL migration. */
ALTER TABLE challenges ADD COLUMN end_announced_at BIGINT NOT NULL DEFAULT 0;
