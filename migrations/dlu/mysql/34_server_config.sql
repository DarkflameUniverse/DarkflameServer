/* Server settings in the database, editable from the dashboard. Servers copy their ini and environment values here
   (file_value, not for secrets) at startup. A web_value is used when the file and environment do not set the key,
   or always when web_wins is set. */
CREATE TABLE IF NOT EXISTS server_config (
    file VARCHAR(64) NOT NULL,
    name VARCHAR(128) NOT NULL,
    file_value TEXT NULL,
    file_source VARCHAR(8) NOT NULL DEFAULT '',
    web_value TEXT NULL,
    web_wins TINYINT NOT NULL DEFAULT 0,
    secret TINYINT NOT NULL DEFAULT 0,
    description TEXT NULL,
    seen_at BIGINT NOT NULL DEFAULT 0,
    updated_at BIGINT NOT NULL DEFAULT 0,
    updated_by VARCHAR(64) NOT NULL DEFAULT '',
    PRIMARY KEY (file, name)
);
