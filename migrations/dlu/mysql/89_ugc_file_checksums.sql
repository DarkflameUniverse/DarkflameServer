/* UGC server: ugc_file_checksums holds the MD5 (lowercase hex) and size of each file it made, as the game client has
   it after inflating the download (kind 0: a player model, storage_id its ugc id; kind 1: a combination of car or
   rocket modules, storage_id the combination's id). ugc_modular_build.combination_id is the combination a build was
   made as (0 until the UGC server has seen it). Worlds read both to answer the client's REQUEST_UGC_MANIFEST_INFO.
   See docs/UgcServer.md. */
CREATE TABLE IF NOT EXISTS ugc_file_checksums (
	kind INT NOT NULL,
	storage_id BIGINT NOT NULL,
	file VARCHAR(32) NOT NULL,
	md5 CHAR(32) NOT NULL,
	size INT UNSIGNED NOT NULL,
	PRIMARY KEY (kind, storage_id, file)
);

SET @dlu_column = (SELECT IF(COUNT(*) = 0, 'ALTER TABLE ugc_modular_build ADD COLUMN combination_id BIGINT NOT NULL DEFAULT 0', 'DO 0') FROM information_schema.columns
	WHERE table_schema = DATABASE() AND table_name = 'ugc_modular_build' AND column_name = 'combination_id');
PREPARE dlu_column_stmt FROM @dlu_column;
EXECUTE dlu_column_stmt;
DEALLOCATE PREPARE dlu_column_stmt;
