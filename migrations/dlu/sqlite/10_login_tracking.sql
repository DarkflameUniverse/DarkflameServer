/* Migration: Add login tracking columns to accounts table */
/* Adds fields for tracking login attempts, lockouts, and last login */

ALTER TABLE accounts ADD COLUMN failed_attempts INTEGER NOT NULL DEFAULT 0;
ALTER TABLE accounts ADD COLUMN lockout_time DATETIME DEFAULT NULL;
ALTER TABLE accounts ADD COLUMN last_login DATETIME DEFAULT NULL;
