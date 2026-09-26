/* scheduled_events name, mode, schedule, priority and parts, and the calendar and vanity events moved into them: see
   the MySQL migration. */
ALTER TABLE scheduled_events ADD COLUMN name TEXT NOT NULL DEFAULT '';
ALTER TABLE scheduled_events ADD COLUMN mode INTEGER NOT NULL DEFAULT 1;
ALTER TABLE scheduled_events ADD COLUMN schedule TEXT NOT NULL DEFAULT '';
ALTER TABLE scheduled_events ADD COLUMN priority INTEGER NOT NULL DEFAULT 0;
ALTER TABLE scheduled_events ADD COLUMN parts TEXT NOT NULL DEFAULT '';
UPDATE scheduled_events SET name = feature, mode = CASE WHEN state = 3 THEN 0 ELSE 1 END,
	parts = json_array(json_object('kind', 'feature', 'config', json_object('feature', feature), 'applied', json(CASE WHEN state = 1 THEN 'true' ELSE 'false' END),
		'state', json_object('slot', slot, 'previousValue', previous_value, 'previousWebWins', json(CASE WHEN previous_web_wins <> 0 THEN 'true' ELSE 'false' END)), 'status', status))
	WHERE parts = '' AND feature <> '';
INSERT INTO scheduled_events (feature, name, note, mode, schedule, starts_at, ends_at, priority, parts, state, status, created_at, created_by, updated_at, updated_by)
	SELECT '', name, note, mode, schedule, 0, 0, priority,
		json_array(json_object('kind', 'vanity', 'config', json_object('file', file, 'removals', removals,
			'fileSwitches', CASE WHEN file_switches = '' THEN json_object() ELSE json(file_switches) END),
			'applied', json(CASE WHEN applied <> 0 THEN 'true' ELSE 'false' END), 'state', json_object(), 'status', '')),
		CASE WHEN applied <> 0 THEN 1 ELSE 0 END, '', created_at, created_by, updated_at, updated_by
	FROM vanity_events;
