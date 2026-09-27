/* The UGC server in the health history: whether master was set to start it (enable_ugc_server) and whether it was up. */
ALTER TABLE server_health ADD COLUMN IF NOT EXISTS ugc_enabled TINYINT NOT NULL DEFAULT 0;
ALTER TABLE server_health ADD COLUMN IF NOT EXISTS ugc_online TINYINT NOT NULL DEFAULT 0;
