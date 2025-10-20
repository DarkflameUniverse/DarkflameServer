update pet_names set id = id % 0x100000000 | 0x1000000000000000;
