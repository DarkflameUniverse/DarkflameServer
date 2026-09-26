/* Periodic dashboard tasks. scheduled_tasks only has rows for tasks whose schedule or switch was changed on the
   dashboard, or that have run on a schedule (last_scheduled_at, used to catch up after downtime).
   scheduled_task_runs keeps finished runs with their log, pruned after log_task_days. */
CREATE TABLE IF NOT EXISTS scheduled_tasks (
    name VARCHAR(64) NOT NULL PRIMARY KEY,
    schedule VARCHAR(128) NULL,
    enabled TINYINT NOT NULL DEFAULT 1,
    last_scheduled_at BIGINT NOT NULL DEFAULT 0,
    updated_at BIGINT NOT NULL DEFAULT 0,
    updated_by VARCHAR(64) NOT NULL DEFAULT ''
);

CREATE TABLE IF NOT EXISTS scheduled_task_runs (
    id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
    task VARCHAR(64) NOT NULL,
    task_trigger VARCHAR(16) NOT NULL,
    actor VARCHAR(64) NOT NULL DEFAULT '',
    started_at BIGINT NOT NULL,
    finished_at BIGINT NOT NULL,
    status TINYINT NOT NULL,
    summary TEXT NULL,
    log MEDIUMTEXT NULL,
    INDEX scheduled_task_runs_task (task, id),
    INDEX scheduled_task_runs_finished (finished_at)
);
