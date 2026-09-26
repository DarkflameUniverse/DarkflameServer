-- Migration: Add login tracking columns to accounts table
-- Adds fields for tracking login attempts, lockouts, and last login

ALTER TABLE accounts ADD COLUMN IF NOT EXISTS failed_attempts INT NOT NULL DEFAULT 0;
ALTER TABLE accounts ADD COLUMN IF NOT EXISTS lockout_time DATETIME NULL DEFAULT NULL;
ALTER TABLE accounts ADD COLUMN IF NOT EXISTS last_login DATETIME NULL DEFAULT NULL;