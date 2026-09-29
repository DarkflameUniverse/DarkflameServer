/* The system description clients send with their login request, as reported by the client. See the MySQL migration. */
CREATE TABLE IF NOT EXISTS client_sysinfo (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    account_id INTEGER NOT NULL,
    first_seen BIGINT NOT NULL,
    last_seen BIGINT NOT NULL,
    logins INTEGER NOT NULL DEFAULT 1,
    ip TEXT NOT NULL DEFAULT '',
    client_os INTEGER NOT NULL DEFAULT 0,
    memory_stats TEXT NOT NULL DEFAULT '',
    memory_total_kb BIGINT NOT NULL DEFAULT 0,
    video_card TEXT NOT NULL DEFAULT '',
    number_of_processors INTEGER NOT NULL DEFAULT 0,
    processor_type INTEGER NOT NULL DEFAULT 0,
    processor_level INTEGER NOT NULL DEFAULT 0,
    processor_revision INTEGER NOT NULL DEFAULT 0,
    os_version_info_size INTEGER NOT NULL DEFAULT 0,
    os_major_version INTEGER NOT NULL DEFAULT 0,
    os_minor_version INTEGER NOT NULL DEFAULT 0,
    os_build_number INTEGER NOT NULL DEFAULT 0,
    os_platform_id INTEGER NOT NULL DEFAULT 0
);
CREATE INDEX IF NOT EXISTS client_sysinfo_account ON client_sysinfo (account_id, id);
CREATE INDEX IF NOT EXISTS client_sysinfo_last_seen ON client_sysinfo (last_seen);
