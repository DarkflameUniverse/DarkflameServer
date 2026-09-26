#pragma once

/**
 * Database backups: the database_backup scheduled task (SQLite via VACUUM INTO, MySQL via mysqldump) and the Backups
 * page's API. Downloading a backup needs the password (and two-factor code) again and is audited.
 */
void RegisterBackupRoutes();

// Before Scheduler::Initialize
void RegisterBackupTask();
