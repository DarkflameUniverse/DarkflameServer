CREATE TABLE IF NOT EXISTS dashboard_audit_log (
    id BIGINT AUTO_INCREMENT PRIMARY KEY,
    timestamp BIGINT NOT NULL,
    ip_address VARCHAR(45) NOT NULL,
    endpoint VARCHAR(255) NOT NULL,
    method VARCHAR(10) NOT NULL,
    user_agent TEXT,
    response_code INT NOT NULL,
    INDEX idx_dashboard_audit_log_timestamp (timestamp),
    INDEX idx_dashboard_audit_log_ip (ip_address)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS dashboard_config (
    config_key VARCHAR(100) PRIMARY KEY,
    config_value TEXT NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT IGNORE INTO dashboard_config (config_key, config_value) VALUES ('rate_limit_enabled', '1');
INSERT IGNORE INTO dashboard_config (config_key, config_value) VALUES ('rate_limit_requests', '100');
INSERT IGNORE INTO dashboard_config (config_key, config_value) VALUES ('rate_limit_window', '60');
INSERT IGNORE INTO dashboard_config (config_key, config_value) VALUES ('audit_retention_days', '90');
