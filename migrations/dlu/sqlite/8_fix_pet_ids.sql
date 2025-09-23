/* Unset the fake persistent bit alongside the Character bit and then re-set the Character bit */
update pet_names set id = id % 0x100000000 | 0x1000000000000000;
