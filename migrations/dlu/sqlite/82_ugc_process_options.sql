/* UGC server processing options: what staff picked for a model's next make, what made it, and every make's options and
   times. See the MySQL migration. */
ALTER TABLE ugc ADD COLUMN process_options TEXT NOT NULL DEFAULT '';
ALTER TABLE ugc ADD COLUMN made_options TEXT NOT NULL DEFAULT '';

CREATE TABLE IF NOT EXISTS ugc_process_runs (
	id INTEGER PRIMARY KEY AUTOINCREMENT,
	ugc_id BIGINT NOT NULL,
	options TEXT NOT NULL,
	made_at BIGINT NOT NULL,
	process_ms INTEGER NOT NULL,
	process_cpu_ms INTEGER NOT NULL,
	hsr_ms INTEGER NOT NULL,
	ao_ms INTEGER NOT NULL,
	icon_ms INTEGER NOT NULL,
	bricks INTEGER NOT NULL,
	triangles_before INTEGER NOT NULL,
	triangles INTEGER NOT NULL
);
CREATE INDEX IF NOT EXISTS ugc_process_runs_options ON ugc_process_runs (options);
CREATE INDEX IF NOT EXISTS ugc_process_runs_ugc_id ON ugc_process_runs (ugc_id);
