ALTER TABLE mail ADD COLUMN sender_id_1 BIGINT DEFAULT 0;
ALTER TABLE bug_reports ADD COLUMN reporter_id_1 BIGINT DEFAULT 0;
/* The leaderboard last_played change is not needed here since sqlite does not have ON UPDATE */
UPDATE activity_log SET character_id = character_id | 0x1000000000000000;
UPDATE behaviors SET character_id = character_id | 0x1000000000000000;
UPDATE bug_reports SET reporter_id_1 = reporter_id | 0x1000000000000000;
UPDATE charinfo SET id = id | 0x1000000000000000;
UPDATE charxml SET id = id | 0x1000000000000000;
UPDATE command_log SET character_id = character_id | 0x1000000000000000;
UPDATE friends SET player_id = player_id | 0x1000000000000000, friend_id = friend_id | 0x1000000000000000;
UPDATE ignore_list SET player_id = player_id | 0x1000000000000000, ignored_player_id = ignored_player_id | 0x1000000000000000;
UPDATE leaderboard SET character_id = character_id | 0x1000000000000000;
UPDATE mail SET sender_id_1 = sender_id | 0x1000000000000000, receiver_id = receiver_id | 0x1000000000000000;
UPDATE properties SET owner_id = owner_id | 0x1000000000000000;
UPDATE ugc SET character_id = character_id | 0x1000000000000000;
UPDATE ugc_modular_build SET character_id = character_id | 0x1000000000000000;

ALTER TABLE mail DROP COLUMN sender_id;
ALTER TABLE mail RENAME COLUMN sender_id_1 TO sender_id;

ALTER TABLE bug_reports DROP COLUMN reporter_id;
ALTER TABLE bug_reports RENAME COLUMN reporter_id_1 TO reporter_id;
