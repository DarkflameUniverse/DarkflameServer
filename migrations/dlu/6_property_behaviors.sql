UPDATE properties_contents SET model_name = "Objects_" || lot || "_name" WHERE model_name = "";
CREATE TABLE IF NOT EXISTS behaviors (behavior_info TEXT NOT NULL, behavior_id BIGINT NOT NULL PRIMARY KEY, character_id BIGINT NOT NULL DEFAULT 0);
