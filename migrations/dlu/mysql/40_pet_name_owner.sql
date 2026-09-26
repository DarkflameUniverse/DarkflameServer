/* Who owns each named pet, written by the game when the name is set. NULL: not looked up yet (the dashboard fills
   those in once from the characters' inventories); 0: no character has the pet any more. */
ALTER TABLE pet_names ADD COLUMN owner_id BIGINT NULL DEFAULT NULL;
CREATE INDEX pet_names_owner ON pet_names (owner_id);
