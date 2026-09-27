/* UGC server: the checksums of the files it made and each car or rocket build's combination. See the MySQL migration. */
CREATE TABLE IF NOT EXISTS ugc_file_checksums (
	kind INTEGER NOT NULL,
	storage_id BIGINT NOT NULL,
	file TEXT NOT NULL,
	md5 TEXT NOT NULL,
	size INTEGER NOT NULL,
	PRIMARY KEY (kind, storage_id, file)
);

ALTER TABLE ugc_modular_build ADD COLUMN combination_id BIGINT NOT NULL DEFAULT 0;
