-- Every brick by brick save as the client sent it (sd0), before the server splits it into one model per group of
-- connected bricks; each model made from it points back to it
CREATE TABLE IF NOT EXISTS ugc_unsplit (
	id BIGINT NOT NULL PRIMARY KEY,
	account_id INT NOT NULL REFERENCES accounts(id),
	character_id BIGINT NOT NULL REFERENCES charinfo(id),
	lxfml MEDIUMBLOB NOT NULL,
	created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP
);

ALTER TABLE ugc ADD COLUMN unsplit_id BIGINT NULL DEFAULT NULL;
