/* UGC server: the CPU time and estimated memory of each model's and build's last make. See the MySQL migration. */
ALTER TABLE ugc ADD COLUMN process_cpu_ms INTEGER NOT NULL DEFAULT 0;
ALTER TABLE ugc ADD COLUMN process_memory_kb INTEGER NOT NULL DEFAULT 0;
ALTER TABLE ugc_modular_build ADD COLUMN process_cpu_ms INTEGER NOT NULL DEFAULT 0;
ALTER TABLE ugc_modular_build ADD COLUMN process_memory_kb INTEGER NOT NULL DEFAULT 0;
