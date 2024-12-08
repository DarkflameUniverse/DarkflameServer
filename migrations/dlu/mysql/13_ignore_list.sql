CREATE TABLE IF NOT EXISTS ignore_list (
	player_id BIGINT NOT NULL REFERENCES charinfo(id) ON DELETE CASCADE,
	ignored_player_id BIGINT NOT NULL REFERENCES charinfo(id) ON DELETE CASCADE,

	PRIMARY KEY (player_id, ignored_player_id)
);
