ALTER TABLE properties_contents
	ADD COLUMN model_name TEXT NOT NULL DEFAULT "",
	ADD COLUMN model_description TEXT NOT NULL DEFAULT "",
	ADD COLUMN behavior_1 INT NOT NULL DEFAULT 0,
	ADD COLUMN behavior_2 INT NOT NULL DEFAULT 0,
	ADD COLUMN behavior_3 INT NOT NULL DEFAULT 0,
	ADD COLUMN behavior_4 INT NOT NULL DEFAULT 0,
	ADD COLUMN behavior_5 INT NOT NULL DEFAULT 0;

UPDATE properties_contents SET model_name = CONCAT("Objects_", lot, "_name") WHERE model_name = "";
CREATE TABLE IF NOT EXISTS behaviors (id INT NOT NULL, behavior_info TEXT NOT NULL);
