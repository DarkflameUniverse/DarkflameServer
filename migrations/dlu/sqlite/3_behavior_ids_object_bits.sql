UPDATE behaviors SET behavior_id = behavior_id | 0x1000000000000000;
ALTER TABLE properties_contents ADD COLUMN behavior_1_1 BIGINT DEFAULT 0;
ALTER TABLE properties_contents ADD COLUMN behavior_2_1 BIGINT DEFAULT 0;
ALTER TABLE properties_contents ADD COLUMN behavior_3_1 BIGINT DEFAULT 0;
ALTER TABLE properties_contents ADD COLUMN behavior_4_1 BIGINT DEFAULT 0;
ALTER TABLE properties_contents ADD COLUMN behavior_5_1 BIGINT DEFAULT 0;
UPDATE properties_contents SET
	behavior_1_1 = behavior_1 | 0x1000000000000000
	WHERE behavior_1 != 0;
UPDATE properties_contents SET
	behavior_2_1 = behavior_2 | 0x1000000000000000
	WHERE behavior_2 != 0;
UPDATE properties_contents SET
	behavior_3_1 = behavior_3 | 0x1000000000000000
	WHERE behavior_3 != 0;
UPDATE properties_contents SET
	behavior_4_1 = behavior_4 | 0x1000000000000000
	WHERE behavior_4 != 0;
UPDATE properties_contents SET
	behavior_5_1 = behavior_5 | 0x1000000000000000
	WHERE behavior_5 != 0;
ALTER TABLE properties_contents DROP COLUMN behavior_1;
ALTER TABLE properties_contents DROP COLUMN behavior_2;
ALTER TABLE properties_contents DROP COLUMN behavior_3;
ALTER TABLE properties_contents DROP COLUMN behavior_4;
ALTER TABLE properties_contents DROP COLUMN behavior_5;
ALTER TABLE properties_contents RENAME COLUMN behavior_1_1 TO behavior_1;
ALTER TABLE properties_contents RENAME COLUMN behavior_2_1 TO behavior_2;
ALTER TABLE properties_contents RENAME COLUMN behavior_3_1 TO behavior_3;
ALTER TABLE properties_contents RENAME COLUMN behavior_4_1 TO behavior_4;
ALTER TABLE properties_contents RENAME COLUMN behavior_5_1 TO behavior_5;
