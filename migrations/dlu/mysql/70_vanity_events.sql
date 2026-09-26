/* vanity_events: changes to the vanity NPCs that are only there while an event is on (see dCommon/VanityEvents.h).
   file: a vanity file (in vanity/, not loaded by root.xml) whose NPCs are added, replacing base NPCs of the same name.
   removals: NPC names taken out while it is on, one per line.
   schedule: JSON rules ({utcOffset, match, rules}); priority: higher is laid on later and wins.
   mode: 0 off, 1 on by its schedule, 2 always on.
   applied: 1 when the dashboard last respawned the worlds with this event on. */
CREATE TABLE IF NOT EXISTS vanity_events (
	id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
	name VARCHAR(100) NOT NULL,
	note TEXT NOT NULL,
	file VARCHAR(64) NOT NULL DEFAULT '',
	removals TEXT NOT NULL,
	schedule TEXT NOT NULL,
	priority INT NOT NULL DEFAULT 0,
	mode TINYINT UNSIGNED NOT NULL DEFAULT 0,
	applied TINYINT NOT NULL DEFAULT 0,
	created_at BIGINT NOT NULL DEFAULT 0,
	created_by VARCHAR(64) NOT NULL DEFAULT '',
	updated_at BIGINT NOT NULL DEFAULT 0,
	updated_by VARCHAR(64) NOT NULL DEFAULT ''
);
