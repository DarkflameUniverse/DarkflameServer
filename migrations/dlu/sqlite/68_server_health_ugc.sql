/* The UGC server in the health history. See the MySQL migration. */
ALTER TABLE server_health ADD COLUMN ugc_enabled INTEGER NOT NULL DEFAULT 0;
ALTER TABLE server_health ADD COLUMN ugc_online INTEGER NOT NULL DEFAULT 0;
