/* Periodic dashboard tasks and their run history. See the MySQL migration. */
CREATE TABLE IF NOT EXISTS scheduled_tasks (
    name TEXT NOT NULL PRIMARY KEY,
    schedule TEXT NULL,
    enabled INTEGER NOT NULL DEFAULT 1,
    last_scheduled_at BIGINT NOT NULL DEFAULT 0,
    updated_at BIGINT NOT NULL DEFAULT 0,
    updated_by TEXT NOT NULL DEFAULT ''
);

CREATE TABLE IF NOT EXISTS scheduled_task_runs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    task TEXT NOT NULL,
    task_trigger TEXT NOT NULL,
    actor TEXT NOT NULL DEFAULT '',
    started_at BIGINT NOT NULL,
    finished_at BIGINT NOT NULL,
    status INTEGER NOT NULL,
    summary TEXT NULL,
    log TEXT NULL
);
CREATE INDEX IF NOT EXISTS scheduled_task_runs_task ON scheduled_task_runs (task, id);
CREATE INDEX IF NOT EXISTS scheduled_task_runs_finished ON scheduled_task_runs (finished_at);
