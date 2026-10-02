/* How much room the game database takes, per sample and per table. See the MySQL migration. */
CREATE TABLE IF NOT EXISTS database_usage (
    time BIGINT NOT NULL PRIMARY KEY,
    total_bytes BIGINT NOT NULL DEFAULT 0,
    free_bytes BIGINT NOT NULL DEFAULT 0,
    data_bytes BIGINT NOT NULL DEFAULT 0,
    index_bytes BIGINT NOT NULL DEFAULT 0,
    row_count BIGINT NOT NULL DEFAULT 0,
    disk_free BIGINT NOT NULL DEFAULT 0,
    disk_total BIGINT NOT NULL DEFAULT 0
);
CREATE TABLE IF NOT EXISTS database_usage_tables (
    time BIGINT NOT NULL,
    table_name TEXT NOT NULL,
    data_bytes BIGINT NOT NULL DEFAULT 0,
    index_bytes BIGINT NOT NULL DEFAULT 0,
    row_count BIGINT NOT NULL DEFAULT 0,
    PRIMARY KEY (time, table_name)
);
