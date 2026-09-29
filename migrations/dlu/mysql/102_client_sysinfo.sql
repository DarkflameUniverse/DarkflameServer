/* The system description clients send with their login request, exactly as the client reported it (old Windows calls:
   compatibility values, not necessarily the player's real hardware). Written by the auth server at each successful
   login: the account's newest row gets last_seen, logins + 1 and the new memory_stats text while everything else is the
   same; otherwise a new row starts. ip is empty while log_login_addresses is off. memory_total_kb is the physical memory
   read from memory_stats (0: not read). Rows not seen for log_client_sysinfo_days are deleted by the log pruning task. */
CREATE TABLE IF NOT EXISTS client_sysinfo (
    id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
    account_id INT UNSIGNED NOT NULL,
    first_seen BIGINT NOT NULL,
    last_seen BIGINT NOT NULL,
    logins INT UNSIGNED NOT NULL DEFAULT 1,
    ip VARCHAR(64) NOT NULL DEFAULT '',
    client_os INT UNSIGNED NOT NULL DEFAULT 0,
    memory_stats VARCHAR(512) NOT NULL DEFAULT '',
    memory_total_kb BIGINT UNSIGNED NOT NULL DEFAULT 0,
    video_card VARCHAR(256) NOT NULL DEFAULT '',
    number_of_processors INT UNSIGNED NOT NULL DEFAULT 0,
    processor_type INT UNSIGNED NOT NULL DEFAULT 0,
    processor_level SMALLINT UNSIGNED NOT NULL DEFAULT 0,
    processor_revision SMALLINT UNSIGNED NOT NULL DEFAULT 0,
    os_version_info_size INT UNSIGNED NOT NULL DEFAULT 0,
    os_major_version INT UNSIGNED NOT NULL DEFAULT 0,
    os_minor_version INT UNSIGNED NOT NULL DEFAULT 0,
    os_build_number INT UNSIGNED NOT NULL DEFAULT 0,
    os_platform_id INT UNSIGNED NOT NULL DEFAULT 0,
    INDEX client_sysinfo_account (account_id, id),
    INDEX client_sysinfo_last_seen (last_seen)
);
