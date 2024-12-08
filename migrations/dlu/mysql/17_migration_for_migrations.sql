update migration_history set name = concat(substr(name, 1, 4), "mysql/", substr(name, 5));
