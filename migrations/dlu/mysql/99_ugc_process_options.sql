/* UGC server processing options (docs/UgcServer.md, "Processing options"): ugc.process_options holds the options staff
   picked for a model's next make (the dashboard's make again, /reprocessproperty), as "embree fast oidn"; empty: the
   UGC settings'. Cleared once the model is made. ugc.made_options is what made the model's current files ("embree
   toolbox off"; empty for models made before). ugc_process_runs has every successful make of a model with its options,
   times and triangles, for the dashboard's comparison of the options. */
SET @dlu_column = (SELECT IF(COUNT(*) = 0, 'ALTER TABLE ugc ADD COLUMN process_options VARCHAR(64) NOT NULL DEFAULT ''''', 'DO 0') FROM information_schema.columns
	WHERE table_schema = DATABASE() AND table_name = 'ugc' AND column_name = 'process_options');
PREPARE dlu_column_stmt FROM @dlu_column;
EXECUTE dlu_column_stmt;
DEALLOCATE PREPARE dlu_column_stmt;

SET @dlu_column = (SELECT IF(COUNT(*) = 0, 'ALTER TABLE ugc ADD COLUMN made_options VARCHAR(64) NOT NULL DEFAULT ''''', 'DO 0') FROM information_schema.columns
	WHERE table_schema = DATABASE() AND table_name = 'ugc' AND column_name = 'made_options');
PREPARE dlu_column_stmt FROM @dlu_column;
EXECUTE dlu_column_stmt;
DEALLOCATE PREPARE dlu_column_stmt;

CREATE TABLE IF NOT EXISTS ugc_process_runs (
	id BIGINT NOT NULL AUTO_INCREMENT PRIMARY KEY,
	ugc_id BIGINT NOT NULL,
	options VARCHAR(64) NOT NULL,
	made_at BIGINT NOT NULL,
	process_ms INT UNSIGNED NOT NULL,
	process_cpu_ms INT UNSIGNED NOT NULL,
	hsr_ms INT UNSIGNED NOT NULL,
	ao_ms INT UNSIGNED NOT NULL,
	icon_ms INT UNSIGNED NOT NULL,
	bricks INT UNSIGNED NOT NULL,
	triangles_before INT UNSIGNED NOT NULL,
	triangles INT UNSIGNED NOT NULL,
	INDEX ugc_process_runs_options (options),
	INDEX ugc_process_runs_ugc_id (ugc_id)
);
