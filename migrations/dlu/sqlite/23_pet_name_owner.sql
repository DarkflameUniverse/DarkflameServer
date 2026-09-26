/* Who owns each named pet. See the MySQL migration. */
ALTER TABLE pet_names ADD COLUMN owner_id BIGINT NULL DEFAULT NULL;
CREATE INDEX IF NOT EXISTS pet_names_owner ON pet_names (owner_id);
