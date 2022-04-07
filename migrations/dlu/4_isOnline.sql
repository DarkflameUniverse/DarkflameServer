ALTER TABLE accounts ADD if not exists is_online tinyint(1) NOT NULL DEFAULT 0;
