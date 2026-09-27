/* Per-minute traffic of each server (packets, bytes, HTTP requests and latency), written by the dashboard once a minute
   from the servers' traffic reports and kept for traffic_days. server: master, auth, chat, dashboard, ugc or
   world:<zone>:<instance>. The latency columns are the HTTP latency percentiles of the minute in microseconds. */
CREATE TABLE IF NOT EXISTS server_traffic (
    time BIGINT NOT NULL,
    server VARCHAR(64) NOT NULL,
    packets_in BIGINT UNSIGNED NOT NULL DEFAULT 0,
    packets_out BIGINT UNSIGNED NOT NULL DEFAULT 0,
    bytes_in BIGINT UNSIGNED NOT NULL DEFAULT 0,
    bytes_out BIGINT UNSIGNED NOT NULL DEFAULT 0,
    resends BIGINT UNSIGNED NOT NULL DEFAULT 0,
    http_requests BIGINT UNSIGNED NOT NULL DEFAULT 0,
    http_4xx BIGINT UNSIGNED NOT NULL DEFAULT 0,
    http_5xx BIGINT UNSIGNED NOT NULL DEFAULT 0,
    http_bytes_out BIGINT UNSIGNED NOT NULL DEFAULT 0,
    latency_p50_us INT UNSIGNED NOT NULL DEFAULT 0,
    latency_p95_us INT UNSIGNED NOT NULL DEFAULT 0,
    latency_p99_us INT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (time, server)
);
