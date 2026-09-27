/* Which kind of pet (its LOT) each named pet is. See the MySQL migration. */
ALTER TABLE pet_names ADD COLUMN pet_lot INTEGER NULL DEFAULT NULL;
