/* Per-minute traffic of each server. See the MySQL migration. */
CREATE TABLE IF NOT EXISTS server_traffic (
    time BIGINT NOT NULL,
    server TEXT NOT NULL,
    packets_in BIGINT NOT NULL DEFAULT 0,
    packets_out BIGINT NOT NULL DEFAULT 0,
    bytes_in BIGINT NOT NULL DEFAULT 0,
    bytes_out BIGINT NOT NULL DEFAULT 0,
    resends BIGINT NOT NULL DEFAULT 0,
    http_requests BIGINT NOT NULL DEFAULT 0,
    http_4xx BIGINT NOT NULL DEFAULT 0,
    http_5xx BIGINT NOT NULL DEFAULT 0,
    http_bytes_out BIGINT NOT NULL DEFAULT 0,
    latency_p50_us INTEGER NOT NULL DEFAULT 0,
    latency_p95_us INTEGER NOT NULL DEFAULT 0,
    latency_p99_us INTEGER NOT NULL DEFAULT 0,
    PRIMARY KEY (time, server)
);
