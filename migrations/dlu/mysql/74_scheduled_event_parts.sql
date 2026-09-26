/* Scheduled events become one system: an event has a name, a mode (0 off, 1 on by its schedule, 2 always on), a
   schedule (recurring rules as JSON, see dCommon/ScheduleRules.h; empty: once, from starts_at to ends_at), a priority
   and parts that switch on and off together (JSON [{kind, config, applied, state, status}], see
   dDashboardServer/routes/EventParts.h): a feature flag, vanity changes, a live event, announcements, a restart.
   The old columns feature, slot, previous_value and previous_web_wins are no longer read: each calendar event becomes
   an event with one feature part holding them. Each vanity event becomes an event with one vanity part; the
   vanity_events table is no longer read either (kept, so nothing is lost). */
ALTER TABLE scheduled_events ADD COLUMN name VARCHAR(100) NOT NULL DEFAULT '';
ALTER TABLE scheduled_events ADD COLUMN mode TINYINT UNSIGNED NOT NULL DEFAULT 1;
ALTER TABLE scheduled_events ADD COLUMN schedule TEXT NOT NULL DEFAULT '';
ALTER TABLE scheduled_events ADD COLUMN priority INT NOT NULL DEFAULT 0;
ALTER TABLE scheduled_events ADD COLUMN parts TEXT NOT NULL DEFAULT '';
UPDATE scheduled_events SET name = feature, mode = CASE WHEN state = 3 THEN 0 ELSE 1 END,
	parts = JSON_ARRAY(JSON_OBJECT('kind', 'feature', 'config', JSON_OBJECT('feature', feature), 'applied', state = 1,
		'state', JSON_OBJECT('slot', slot, 'previousValue', previous_value, 'previousWebWins', previous_web_wins <> 0), 'status', status))
	WHERE parts = '' AND feature <> '';
INSERT INTO scheduled_events (feature, name, note, mode, schedule, starts_at, ends_at, priority, parts, state, status, created_at, created_by, updated_at, updated_by)
	SELECT '', name, note, mode, schedule, 0, 0, priority,
		JSON_ARRAY(JSON_OBJECT('kind', 'vanity', 'config', JSON_OBJECT('file', file, 'removals', removals,
			'fileSwitches', CASE WHEN file_switches = '' THEN JSON_OBJECT() ELSE JSON_EXTRACT(file_switches, '$') END),
			'applied', applied <> 0, 'state', JSON_OBJECT(), 'status', '')),
		CASE WHEN applied <> 0 THEN 1 ELSE 0 END, '', created_at, created_by, updated_at, updated_by
	FROM vanity_events;
