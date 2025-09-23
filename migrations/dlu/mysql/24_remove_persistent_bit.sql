START TRANSACTION;
CREATE TABLE `ugc_2` (
  `id` BIGINT NOT NULL PRIMARY KEY,
  `account_id` INT(11) NOT NULL REFERENCES `accounts` (`id`),
  `character_id` BIGINT(20) NOT NULL REFERENCES `charinfo` (`id`),
  `is_optimized` TINYINT(1) NOT NULL DEFAULT 0,
  `lxfml` MEDIUMBLOB NOT NULL,
  `bake_ao` TINYINT(1) NOT NULL DEFAULT 0,
  `filename` TEXT NOT NULL DEFAULT ''
);
CREATE TABLE `properties_contents_2` (
  `id` BIGINT PRIMARY KEY NOT NULL,
  `property_id` BIGINT NOT NULL REFERENCES `properties`(`id`),
  `ugc_id` BIGINT DEFAULT NULL REFERENCES `ugc_2`(`id`),
  `lot` INT NOT NULL,
  `x` FLOAT NOT NULL,
  `y` FLOAT NOT NULL,
  `z` FLOAT NOT NULL,
  `rx` FLOAT NOT NULL,
  `ry` FLOAT NOT NULL,
  `rz` FLOAT NOT NULL,
  `rw` FLOAT NOT NULL,
  `model_name` TEXT NOT NULL DEFAULT '',
  `model_description` TEXT NOT NULL DEFAULT '',
  `behavior_1` BIGINT DEFAULT 0,
  `behavior_2` BIGINT DEFAULT 0,
  `behavior_3` BIGINT DEFAULT 0,
  `behavior_4` BIGINT DEFAULT 0,
  `behavior_5` BIGINT DEFAULT 0
);
INSERT INTO `ugc_2` SELECT `id`|0x1000000000000000,`account_id`,`character_id`,`is_optimized`,`lxfml`,`bake_ao`,`filename` FROM `ugc`;
INSERT INTO `properties_contents_2` SELECT `id`,`property_id`,`ugc_id`|0x1000000000000000,`lot`,`x`,`y`,`z`,`rx`,`ry`,`rz`,`rw`,`model_name`,`model_description`,`behavior_1`,`behavior_2`,`behavior_3`,`behavior_4`,`behavior_5` FROM `properties_contents`;
DROP TABLE `properties_contents`;
DROP TABLE `ugc`;
RENAME TABLE `properties_contents_2` TO `properties_contents`;
RENAME TABLE `ugc_2` TO `ugc`;
COMMIT;
