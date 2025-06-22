UPDATE behaviors SET behavior_id = behavior_id | 0x1000000000000000;
ALTER TABLE properties_contents MODIFY behavior_1 BIGINT DEFAULT 0;
ALTER TABLE properties_contents MODIFY behavior_2 BIGINT DEFAULT 0;
ALTER TABLE properties_contents MODIFY behavior_3 BIGINT DEFAULT 0;
ALTER TABLE properties_contents MODIFY behavior_4 BIGINT DEFAULT 0;
ALTER TABLE properties_contents MODIFY behavior_5 BIGINT DEFAULT 0;
UPDATE properties_contents SET
	behavior_1 = behavior_1 | 0x1000000000000000
	WHERE behavior_1 != 0;
UPDATE properties_contents SET
	behavior_2 = behavior_2 | 0x1000000000000000
	WHERE behavior_2 != 0;
UPDATE properties_contents SET
	behavior_3 = behavior_3 | 0x1000000000000000
	WHERE behavior_3 != 0;
UPDATE properties_contents SET
	behavior_4 = behavior_4 | 0x1000000000000000
	WHERE behavior_4 != 0;
UPDATE properties_contents SET
	behavior_5 = behavior_5 | 0x1000000000000000
	WHERE behavior_5 != 0;
