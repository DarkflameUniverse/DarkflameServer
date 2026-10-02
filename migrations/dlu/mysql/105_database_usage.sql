/* How much room the game database takes, sampled by the dashboard's Database usage task and kept for
   database_usage_days. database_usage: the whole database per sample (sizes in bytes; disk_total 0 when the database's
   volume is not known); database_usage_tables: each table at that sample. row_count is the server's estimate on MySQL. */
CREATE TABLE IF NOT EXISTS database_usage (
    time BIGINT NOT NULL PRIMARY KEY,
    total_bytes BIGINT UNSIGNED NOT NULL DEFAULT 0,
    free_bytes BIGINT UNSIGNED NOT NULL DEFAULT 0,
    data_bytes BIGINT UNSIGNED NOT NULL DEFAULT 0,
    index_bytes BIGINT UNSIGNED NOT NULL DEFAULT 0,
    row_count BIGINT UNSIGNED NOT NULL DEFAULT 0,
    disk_free BIGINT UNSIGNED NOT NULL DEFAULT 0,
    disk_total BIGINT UNSIGNED NOT NULL DEFAULT 0
);
CREATE TABLE IF NOT EXISTS database_usage_tables (
    time BIGINT NOT NULL,
    table_name VARCHAR(64) NOT NULL,
    data_bytes BIGINT UNSIGNED NOT NULL DEFAULT 0,
    index_bytes BIGINT UNSIGNED NOT NULL DEFAULT 0,
    row_count BIGINT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (time, table_name)
);
