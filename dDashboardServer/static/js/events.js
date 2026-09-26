/**
 * The Scheduled Events page.
 *
 * - The events, with what each switches on (its parts, and what each part last did), when it is on and next turns on
 *   and off, and a mode switch (off, by its schedule, always on). Then a month calendar with the events and the things
 *   scheduled on their own pages, the event_N settings, and the events that change the same vanity NPCs or files.
 * - The editor: name, mode, priority; once (two times) or by rules (every year, once, days of the week, times of day,
 *   moon phases, groups; or JSON, see dCommon/ScheduleRules.h), which the server checks and lists as you type; and the
 *   parts, one editor per kind. Kinds the user may not add are shown but can't be picked.
 * - #event:ID opens that event, #new a new one, #new:vanity a new one with a vanity part (the Vanity page links here).
 */
(function () {
	'use strict';

	var DAYS = ['sun', 'mon', 'tue', 'wed', 'thu', 'fri', 'sat'];
	var PHASES = [['new_moon', 'New moon'], ['first_quarter', 'First quarter'], ['full_moon', 'Full moon'], ['last_quarter', 'Last quarter']];
	var RULE_LABELS = { yearly: 'Every year', dates: 'Once', weekdays: 'Days of the week', time_of_day: 'Times of day', moon: 'Moon phase', group: 'Group' };
	var KIND_LABELS = { feature: 'Game feature', vanity: 'Vanity changes', live_event: 'Live event', announcement: 'Announcement', restart: 'Restart' };
	var LIVE_TYPES = [['treasure_hunt', 'Treasure hunt'], ['bonus', 'Bonus'], ['invasion', 'Invasion'], ['celebration', 'Celebration']];
	// Colours by state name, so the numbers stay the server's
	var COLOURS = { 'Scheduled': 'info', 'Active': 'success', 'Ended': 'secondary', 'Cancelled': 'secondary', 'Missed': 'warning' };

	var data = { events: [], features: [], slots: [], states: [], modes: [], kinds: [], zones: [], vanity: null };
	var editing = null; // the event being edited, or null for a new one
	var rules = [];     // the schedule's rules as edited (the JSON format)
	var parts = [];     // the parts as edited: [{kind, config, applied, status}]
	var pickers = null; // vanity files and NPC names, fetched when a vanity part is edited
	var recentLive = null; // recent live events to copy from
	var previewTimer = null;
	var cal = { shown: new Date(), range: '', list: [] };
	cal.shown.setDate(1);
	var modal = new bootstrap.Modal(document.getElementById('evModal'));
	var importModal = new bootstrap.Modal(document.getElementById('evImportModal'));
	var canShutDown = DASH.can('worlds_manage');

	function get(id) { return document.getElementById(id); }
	function byId(id) { return data.events.filter(function (e) { return String(e.id) === String(id); })[0]; }
	function pad(n) { return (n < 10 ? '0' : '') + n; }
	function lines(text) { return String(text).split('\n').map(function (l) { return l.trim(); }).filter(Boolean); }
	function allowed(kind) { var k = data.kinds.filter(function (x) { return x.name === kind; })[0]; return !!(k && k.allowed); }
	function permissionOf(kind) { return (data.kinds.filter(function (x) { return x.name === kind; })[0] || {}).permission || ''; }
	function colour(e) { return COLOURS[e.stateName] || 'secondary'; }
	function isOver(e) { return e.once && (e.stateName === 'Ended' || e.stateName === 'Missed' || e.stateName === 'Cancelled'); }
	function feature(name) { return data.features.filter(function (f) { return f.name === name; })[0]; }
	// Whether the user may change the event: every kind of part it has needs its permission
	function mayChange(e) { return e.parts.every(function (p) { return allowed(p.kind); }); }

	function offsetLabel(minutes) {
		if (!minutes) return 'UTC';
		var sign = minutes < 0 ? '-' : '+', abs = Math.abs(minutes);
		return 'UTC' + sign + pad(Math.floor(abs / 60)) + ':' + pad(abs % 60);
	}
	// The browser's offset now, to the quarter hour (a new schedule starts in it)
	function browserOffset() { return Math.round(-new Date().getTimezoneOffset() / 15) * 15; }

	function toInput(ts) {
		if (!ts) return '';
		var d = new Date(ts * 1000);
		d.setMinutes(d.getMinutes() - d.getTimezoneOffset());
		return d.toISOString().slice(0, 16);
	}
	function fromInput(value) { return value ? Math.floor(new Date(value).getTime() / 1000) : 0; }

	// ---------------------------------------------------------------- describing schedules and parts

	function monthDay(text) {
		var p = String(text || '').split('-');
		var d = new Date(2000, Number(p[0]) - 1, Number(p[1]));
		return isNaN(d) ? text : d.toLocaleDateString([], { month: 'short', day: 'numeric' });
	}

	function describeRule(r) {
		var text;
		switch (r.type) {
		case 'yearly': text = 'every year ' + monthDay(r.from) + ' to ' + monthDay(r.to); break;
		case 'dates': text = String(r.from).replace('T', ' ') + ' to ' + String(r.to).replace('T', ' '); break;
		case 'weekdays': text = (r.days || []).map(function (d) { return d.charAt(0).toUpperCase() + d.slice(1); }).join(', '); break;
		case 'time_of_day': text = r.from + ' to ' + r.to; break;
		case 'moon': {
			var phase = (PHASES.filter(function (p) { return p[0] === r.phase; })[0] || [r.phase, r.phase])[1].toLowerCase();
			text = r.hours ? phase + ' ±' + r.hours + 'h' : phase + (r.days ? ' day ±' + r.days + 'd' : ' day');
			break;
		}
		case 'group': text = '(' + describeRules(r.rules || [], r.match) + ')'; break;
		default: text = r.type;
		}
		return (r.not ? 'not ' : '') + text;
	}

	function describeRules(list, match) { return list.map(describeRule).join(match === 'all' ? ' and ' : ' or '); }

	// Always with the rules' time zone: a UTC day is two days in most browsers
	function describe(schedule) {
		if (!schedule) return '';
		return describeRules(schedule.rules || [], schedule.match) + ' (' + offsetLabel(schedule.utcOffset || 0) + ')';
	}

	function partSummary(p) {
		var c = p.config || {};
		switch (p.kind) {
		case 'feature': return esc(c.feature || '?') + (p.state && p.state.slot ? ' <code>' + esc('event_' + p.state.slot) + '</code>' : '');
		case 'vanity': {
			var bits = [];
			Object.keys(c.fileSwitches || {}).forEach(function (f) { bits.push(esc(f) + (c.fileSwitches[f] ? ' on' : ' off')); });
			if (c.file) bits.push('overlay ' + esc(c.file));
			var removals = Array.isArray(c.removals) ? c.removals : lines(c.removals || '');
			if (removals.length) bits.push('takes out ' + esc(removals.join(', ')));
			return bits.join('; ');
		}
		case 'live_event': return esc((LIVE_TYPES.filter(function (t) { return t[0] === c.type; })[0] || ['', c.type])[1]) + (c.title ? ' &ldquo;' + esc(c.title) + '&rdquo;' : '');
		case 'announcement': return (c.atStart !== false ? 'at the start' : '') + (c.repeat ? (c.atStart !== false ? ', ' : '') + 'repeats <code>' + esc(c.repeat) + '</code>' : '') +
			(c.endMessage ? ', at the end' : '');
		case 'restart': return 'when it ' + (c.when === 'start' ? 'starts' : 'ends') + ', ' + esc(c.minutes) + ' min warning';
		}
		return '';
	}

	function partBadges(e) {
		if (e.partsError) return '<div class="small text-danger">' + esc(e.partsError) + '</div>';
		return '<div class="ev-parts d-flex flex-wrap gap-1 mt-1">' + e.parts.map(function (p) {
			return '<span class="badge ' + (p.applied ? 'text-bg-success' : 'bg-body-secondary text-body border') + '" title="' + esc((p.applied ? 'On' : 'Off') + (p.status ? ': ' + p.status : '')) + '">' +
				esc(KIND_LABELS[p.kind] || p.kind) + (partSummary(p) ? ': <span class="fw-normal">' + partSummary(p) + '</span>' : '') + '</span>';
		}).join('') + '</div>' + e.parts.filter(function (p) { return /^(Waiting|Couldn't|Failed|Not scheduled)/.test(p.status || ''); }).map(function (p) {
			return '<div class="small text-warning-emphasis">' + esc(KIND_LABELS[p.kind] + ': ' + p.status) + '</div>';
		}).join('');
	}

	// ---------------------------------------------------------------- the list

	function whenCell(e) {
		if (e.once) return esc(fmt.unix(e.startsAt)) + '<br>to ' + esc(fmt.unix(e.endsAt));
		if (e.scheduleError) return '<span class="text-danger">' + esc(e.scheduleError) + '</span>';
		return esc(describe(e.schedule));
	}

	function nextCell(e) {
		var state = e.once ? fmt.badge(e.stateName, colour(e)) + ' ' : '';
		if (e.mode === 0) return state + '<span class="text-body-secondary small">Off</span>';
		var now = e.on ? fmt.badge('On', 'success') : fmt.badge('Off now', 'secondary');
		if (e.mode === 2) return now + ' <span class="small text-body-secondary">always</span>';
		if (isOver(e)) return state;
		var bits = [];
		if (e.on && e.nextEnd) bits.push('until ' + esc(fmt.unix(e.nextEnd)));
		if (!e.on && e.nextStart) bits.push('from ' + esc(fmt.unix(e.nextStart)) + (e.nextEnd ? '<br>to ' + esc(fmt.unix(e.nextEnd)) : ''));
		if (e.on && e.nextStart) bits.push('then from ' + esc(fmt.unix(e.nextStart)));
		if (!bits.length && !e.once) bits.push(e.on ? 'for the next four years' : 'not in the next four years');
		return now + '<div class="small">' + bits.join('<br>') + '</div>';
	}

	function worlds(e) {
		// Feature parts: running worlds of the zones that keep what they loaded until restarted
		var list = [];
		e.parts.forEach(function (p) { if (p.kind === 'feature' && p.applied) (p.affected || []).forEach(function (w) { list.push(w); }); });
		if (!list.length) return '';
		return '<div class="small mt-1">Worlds to restart for the feature: ' + list.map(function (w) {
			return fmt.zone(w.zone, w.zoneName) + ' #' + esc(w.instance) + ' (' + esc(w.players) + ' player(s))' +
				(canShutDown ? ' <button type="button" class="btn btn-link btn-sm p-0 align-baseline" data-shutdown="' + w.zone + '/' + w.instance + '">Shut down</button>' : '');
		}).join(', ') + '</div>';
	}

	function renderList() {
		var filter = get('evFilter').value.trim().toLowerCase(), showOver = get('evShowOver').checked;
		var events = data.events.filter(function (e) {
			if (!showOver && isOver(e)) return false;
			return !filter || (e.name + ' ' + e.note + ' ' + JSON.stringify(e.parts.map(function (p) { return p.config; }))).toLowerCase().indexOf(filter) !== -1;
		}).sort(function (a, b) { return (b.on - a.on) || ((a.nextStart || 9e12) - (b.nextStart || 9e12)) || a.id - b.id; });
		get('evRows').innerHTML = events.map(function (e) {
			var may = mayChange(e), lock = may ? '' : ' disabled title="' + esc('Needs ' + e.parts.filter(function (p) { return !allowed(p.kind); }).map(function (p) { return p.permission; }).join(', ')) + '"';
			var modes = data.modes.map(function (m) { return '<option value="' + m.value + '"' + (m.value === e.mode ? ' selected' : '') + '>' + esc(m.name) + '</option>'; }).join('');
			var hasVanity = e.parts.some(function (p) { return p.kind === 'vanity'; });
			return '<tr><td data-label="Event"><strong>' + esc(e.name) + '</strong>' + (e.note ? '<div class="small">' + esc(e.note) + '</div>' : '') + partBadges(e) + worlds(e) + '</td>' +
				'<td data-label="When" class="small">' + whenCell(e) + '</td>' +
				'<td data-label="Next">' + nextCell(e) + '</td>' +
				'<td data-label="Priority">' + esc(e.priority) + '</td>' +
				'<td data-label="Mode"><select class="form-select form-select-sm ev-mode" data-mode="' + e.id + '" aria-label="Mode of ' + esc(e.name) + '"' + lock + '>' + modes + '</select></td>' +
				'<td class="text-end text-nowrap">' +
				'<button type="button" class="btn btn-sm btn-outline-primary me-1" data-edit="' + e.id + '">' + (may ? 'Edit' : 'View') + '</button>' +
				(hasVanity && DASH.can('vanity_manage') ? '<a class="btn btn-sm btn-outline-secondary me-1" href="/vanity#preview:' + e.id + '" title="What the worlds load with this event on">Preview</a>' : '') +
				'<button type="button" class="btn btn-sm btn-outline-secondary me-1" data-export="' + e.id + '">Export</button>' +
				(e.once && !isOver(e) ? '<button type="button" class="btn btn-sm btn-outline-warning me-1" data-cancel="' + e.id + '"' + lock + '>' + (e.on ? 'End now' : 'Cancel') + '</button>' : '') +
				'<button type="button" class="btn btn-sm btn-outline-danger" data-delete="' + e.id + '"' + lock + '>Delete</button></td></tr>';
		}).join('') || '<tr><td colspan="6" class="text-body-secondary">' + (data.events.length ? 'Nothing matches.' : 'No events yet.') + '</td></tr>';
	}

	function renderVanity() {
		var v = data.vanity;
		var items = [];
		if (v) {
			v.conflicts.forEach(function (c) { items.push('<li>Vanity NPC <strong>' + esc(c.npc) + '</strong> is changed by ' + esc(c.events.join(', ')) + '; ' + esc(c.winner) + ' wins.</li>'); });
			v.fileConflicts.forEach(function (c) {
				items.push('<li>Vanity file <strong>' + esc(c.file) + '</strong> is switched by ' + esc(c.switches.map(function (s) { return s.event + ' (' + (s.on ? 'on' : 'off') + ')'; }).join(', ')) +
					'; ' + esc(c.winner) + ' wins, so it is ' + (c.on ? 'on' : 'off') + '.</li>');
			});
		}
		get('evConflicts').innerHTML = items.length ? 'Events that are on now change the same vanity NPCs or files:<ul class="mb-0">' + items.join('') + '</ul>' : '';
		get('evConflicts').classList.toggle('d-none', !items.length);
		var overlaps = v ? v.overlaps : [];
		get('evOverlaps').innerHTML = overlaps.map(function (o) {
			var a = byId(o.a) || {}, b = byId(o.b) || {};
			var what = [];
			if (o.npcs.length) what.push('NPCs ' + o.npcs.join(', '));
			if (o.files.length) what.push('files ' + o.files.join(', '));
			return '<li class="list-group-item"><strong>' + esc(a.name) + '</strong> and <strong>' + esc(b.name) + '</strong> both change ' + esc(what.join(' and ')) +
				'. When both are on, ' + esc((byId(o.winner) || {}).name) + ' wins.</li>';
		}).join('');
		get('evOverlapsCard').classList.toggle('d-none', !overlaps.length);
	}

	function renderSlots() {
		get('slots').innerHTML = data.slots.map(function (s) {
			var values = s.values.map(function (v) {
				return '<code>' + esc(v.value) + '</code> <span class="text-body-secondary">(' + esc(v.source === 'web' ? 'Settings page' : v.source === 'env' ? 'environment' : v.file) + ')</span>';
			}).join(', ');
			var owner = s.event ? byId(s.event) : null;
			return '<li class="mb-1"><code>' + esc(s.setting) + '</code>: ' + (values || '<span class="text-body-secondary">free</span>') +
				(owner ? ' ' + fmt.badge(owner.name, 'success') : '') + '</li>';
		}).join('');
	}

	function load() {
		return api.get('/api/events').then(function (d) {
			if (!d.success) { toast(d.error || 'Could not load the events', 'danger'); return; }
			data = d;
			get('scanning').classList.toggle('d-none', !d.scanning);
			get('clientVersion').textContent = d.clientVersion;
			renderList();
			renderVanity();
			renderSlots();
			cal.range = '';
			renderCalendar();
			renderNewPartOptions();
		});
	}

	// ---------------------------------------------------------------- the calendar

	function renderCalendar() {
		var year = cal.shown.getFullYear(), month = cal.shown.getMonth();
		get('calTitle').textContent = cal.shown.toLocaleDateString([], { month: 'long', year: 'numeric' });
		var first = new Date(year, month, 1);
		var start = new Date(first); start.setDate(1 - ((first.getDay() + 6) % 7)); // weeks start on Monday
		var html = '';
		for (var i = 0; i < 7; i++) html += '<div class="cal-head">' + esc(new Date(2024, 0, 1 + i).toLocaleDateString([], { weekday: 'short' })) + '</div>';
		var today = new Date(); today.setHours(0, 0, 0, 0);
		for (var d = 0; d < 42; d++) {
			var day = new Date(start); day.setDate(start.getDate() + d);
			var from = day.getTime() / 1000, to = from + 86400;
			var chips = cal.list.filter(function (o) { return o.start < to && (o.end > from || (o.end === o.start && o.start >= from)); }).map(function (o) {
				var title = (o.kind === 'event' ? '' : (o.kind === 'live_event' ? 'Live event ' : o.kind === 'announcement' ? 'Announcement ' : '')) + o.name + ': ' + fmt.unix(o.start) +
					(o.end !== o.start ? ' to ' + fmt.unix(o.end) : '');
				if (o.kind !== 'event') return '<a class="cal-event cal-other" href="' + esc(o.link) + '" title="' + esc(title) + '">' + esc(o.name) + '</a>';
				var e = byId(o.id) || {};
				return '<span class="cal-event text-bg-' + (e.on && o.start <= Date.now() / 1000 && o.end > Date.now() / 1000 ? 'success' : 'info') + '" data-open="' + o.id + '" title="' + esc(title) + '">' + esc(o.name) + '</span>';
			}).join('');
			html += '<div class="cal-day' + (day.getMonth() !== month ? ' other' : '') + (day.getTime() === today.getTime() ? ' today' : '') + '"><span class="cal-num">' + day.getDate() + '</span>' + chips + '</div>';
		}
		get('calendar').innerHTML = html;
		loadOccurrences(start.getTime() / 1000, start.getTime() / 1000 + 42 * 86400);
	}

	// When everything is on in the days shown, fetched once per month shown
	function loadOccurrences(from, to) {
		var range = Math.floor(from) + '-' + Math.floor(to);
		if (cal.range === range) return;
		cal.range = range;
		api.get('/api/events/occurrences?from=' + Math.floor(from) + '&to=' + Math.floor(to)).then(function (d) {
			if (!d.success || cal.range !== range) return;
			cal.list = d.occurrences;
			renderCalendar();
		});
	}

	// ---------------------------------------------------------------- the schedule builder

	function newRule(type) {
		switch (type) {
		case 'yearly': return { type: type, from: '10-01', to: '10-31' };
		case 'dates': {
			var today = new Date();
			var iso = today.getFullYear() + '-' + pad(today.getMonth() + 1) + '-' + pad(today.getDate());
			return { type: type, from: iso, to: iso };
		}
		case 'weekdays': return { type: type, days: ['sat', 'sun'] };
		case 'time_of_day': return { type: type, from: '18:00', to: '23:00' };
		case 'moon': return { type: type, phase: 'full_moon', days: 0 };
		default: return { type: 'group', match: 'all', rules: [{ type: 'weekdays', days: ['fri', 'sat'] }, { type: 'time_of_day', from: '18:00', to: '23:59' }] };
		}
	}

	function ruleInput(i, field, value, attrs) {
		return '<input class="form-control form-control-sm" data-i="' + i + '" data-field="' + field + '" value="' + esc(value === undefined ? '' : value) + '" ' + (attrs || '') + '>';
	}

	function ruleBody(r, i) {
		switch (r.type) {
		case 'yearly':
			return 'from ' + ruleInput(i, 'from', r.from, 'placeholder="MM-DD" size="6" aria-label="From (month-day)"') +
				' to ' + ruleInput(i, 'to', r.to, 'placeholder="MM-DD" size="6" aria-label="To (month-day)"') +
				'<span class="text-body-secondary small">both days included; may run past the new year (12-20 to 01-02)</span>';
		case 'dates':
			return 'from ' + ruleInput(i, 'from', r.from, 'placeholder="2026-12-20" size="16" aria-label="From"') +
				' to ' + ruleInput(i, 'to', r.to, 'placeholder="2027-01-02" size="16" aria-label="To"') +
				'<span class="text-body-secondary small">a date is the whole day; add a time as 2026-12-20T18:00</span>';
		case 'weekdays':
			return '<div class="btn-group btn-group-sm ev-days" role="group" aria-label="Days">' + DAYS.map(function (d) {
				var id = 'ev-day-' + i + '-' + d, on = (r.days || []).indexOf(d) !== -1;
				return '<input type="checkbox" class="btn-check" id="' + id + '" data-i="' + i + '" data-day="' + d + '"' + (on ? ' checked' : '') + ' autocomplete="off">' +
					'<label class="btn btn-outline-primary" for="' + id + '">' + d.charAt(0).toUpperCase() + d.slice(1) + '</label>';
			}).join('') + '</div>';
		case 'time_of_day':
			return 'from ' + ruleInput(i, 'from', r.from, 'type="time" aria-label="From"') + ' to ' + ruleInput(i, 'to', r.to, 'type="time" aria-label="To"') +
				'<span class="text-body-secondary small">may run past midnight</span>';
		case 'moon': {
			var phases = PHASES.map(function (p) { return '<option value="' + p[0] + '"' + (p[0] === r.phase ? ' selected' : '') + '>' + p[1] + '</option>'; }).join('');
			var byHours = r.hours !== undefined;
			return '<select class="form-select form-select-sm" data-i="' + i + '" data-field="phase" aria-label="Phase">' + phases + '</select>' +
				'<select class="form-select form-select-sm" data-i="' + i + '" data-window aria-label="How long">' +
				'<option value="days"' + (byHours ? '' : ' selected') + '>the day it falls on, and days either side:</option>' +
				'<option value="hours"' + (byHours ? ' selected' : '') + '>hours either side of the exact time:</option></select>' +
				(byHours ? ruleInput(i, 'hours', r.hours, 'type="number" min="1" max="240" aria-label="Hours"') : ruleInput(i, 'days', r.days || 0, 'type="number" min="0" max="7" aria-label="Days"')) +
				'<span class="text-body-secondary small">"the day" is the day in the rules\' time zone</span>';
		}
		case 'group':
			return '<textarea class="form-control form-control-sm font-monospace w-100" rows="4" data-i="' + i + '" data-group spellcheck="false" aria-label="Group as JSON">' +
				esc(JSON.stringify({ match: r.match, rules: r.rules }, null, 1)) + '</textarea>' +
				'<span class="text-body-secondary small">{"match": "any" or "all", "rules": [...]}, the same rules as JSON</span>';
		}
		return '';
	}

	function renderRules() {
		get('evRules').innerHTML = rules.map(function (r, i) {
			return '<div class="ev-rule"><div class="d-flex flex-wrap align-items-center gap-2">' +
				'<strong class="small">' + esc(RULE_LABELS[r.type] || r.type) + '</strong>' + ruleBody(r, i) +
				'<div class="form-check ms-auto mb-0"><input class="form-check-input" type="checkbox" id="ev-not-' + i + '" data-i="' + i + '" data-not' + (r.not ? ' checked' : '') + '>' +
				'<label class="form-check-label small" for="ev-not-' + i + '">not</label></div>' +
				'<button type="button" class="btn btn-sm btn-outline-danger" data-remove-rule="' + i + '" aria-label="Remove this rule">&times;</button></div></div>';
		}).join('') || '<p class="small text-body-secondary">Add at least one rule.</p>';
	}

	function builderSchedule() { return { utcOffset: Number(get('evOffset').value) || 0, match: get('evMatch').value, rules: rules }; }

	function repeating() { return get('evRepeat').checked; }

	// The schedule as edited (null for once), or undefined (after a toast) when the JSON can't be read
	function currentSchedule(quiet) {
		if (!repeating()) return null;
		if (!get('evJsonMode').checked) return builderSchedule();
		try { return JSON.parse(get('evJson').value); } catch (e) {
			if (!quiet) toast('The schedule isn\'t valid JSON: ' + e.message, 'danger');
			return undefined;
		}
	}

	function offsetInfo() {
		var offset = Number(get('evOffset').value) || 0, mine = browserOffset();
		get('evOffsetInfo').textContent = 'Dates, days and times in the rules are ' + offsetLabel(offset) + ' (a fixed offset: no daylight saving)' +
			(offset === mine ? ', the same as your browser now.' : '; your browser is ' + offsetLabel(mine) + ', so a day in the rules is ' + offsetLabel(offset) + ' midnight to midnight.');
	}

	function loadBuilder(schedule) {
		schedule = schedule || { utcOffset: browserOffset(), match: 'any', rules: [] };
		get('evOffset').value = String(schedule.utcOffset || 0);
		get('evMatch').value = schedule.match === 'all' ? 'all' : 'any';
		rules = (schedule.rules || []).slice();
		renderRules();
		offsetInfo();
	}

	function schedulePreview() {
		clearTimeout(previewTimer);
		var out = get('evSchedulePreview');
		if (!repeating()) {
			var s = fromInput(get('evStarts').value), e = fromInput(get('evEnds').value);
			out.innerHTML = s && e ? (e <= s ? '<span class="text-danger">The end has to be after the start.</span>' : 'On from ' + esc(fmt.unix(s)) + ' to ' + esc(fmt.unix(e)) + ' (your time zone).') : '';
			return;
		}
		previewTimer = setTimeout(function () {
			var schedule = currentSchedule(true);
			if (!schedule) { out.innerHTML = '<span class="text-danger">Not valid JSON</span>'; return; }
			api.post('/api/events/check', { schedule: schedule, count: 6 }).then(function (d) {
				if (!d.success) return;
				if (!d.valid) { out.innerHTML = '<span class="text-danger">' + esc(d.error) + '</span>'; return; }
				out.innerHTML = '<div class="mb-1">' + (d.on ? fmt.badge('On now', 'success') : fmt.badge('Off now', 'secondary')) + ' ' + esc(describe(d.schedule)) + '</div>' +
					(d.windows.length ? '<div class="text-body-secondary">On (shown in your time zone, ' + esc(offsetLabel(browserOffset())) + '):</div><ul class="mb-0">' + d.windows.map(function (w) {
						return '<li>' + esc(fmt.unix(w.start)) + ' to ' + esc(fmt.unix(w.end)) + '</li>';
					}).join('') + '</ul>' : '<div class="text-warning">Not on in the next four years.</div>');
			});
		}, 300);
	}

	function setWhen(repeat) {
		get(repeat ? 'evRepeat' : 'evOnce').checked = true;
		get('evOnceBox').classList.toggle('d-none', repeat);
		get('evRepeatBox').classList.toggle('d-none', !repeat);
		schedulePreview();
	}

	// ---------------------------------------------------------------- the parts

	function renderNewPartOptions() {
		get('evNewPart').innerHTML = data.kinds.map(function (k) {
			return '<option value="' + esc(k.name) + '"' + (k.allowed ? '' : ' disabled') + '>' + esc(KIND_LABELS[k.name] || k.name) + (k.allowed ? '' : ' (needs ' + esc(k.permission) + ')') + '</option>';
		}).join('');
		var first = data.kinds.filter(function (k) { return k.allowed; })[0];
		if (first) get('evNewPart').value = first.name;
	}

	function defaultConfig(kind) {
		switch (kind) {
		case 'feature': return { feature: '' };
		case 'vanity': return { file: '', removals: [], fileSwitches: {} };
		case 'live_event': return { type: 'celebration', title: '', message: '', zones: [], instance: -1, config: liveDefaults('celebration') };
		case 'announcement': return { title: '', message: '', zones: [], atStart: true, repeat: '', endMessage: '' };
		case 'restart': return { when: 'end', minutes: 15, reason: '' };
		}
		return {};
	}

	function liveDefaults(type) {
		switch (type) {
		case 'treasure_hunt': return { lot: 0, count: 10, coins: 0, radius: 4, itemLot: 0, itemCount: 1 };
		case 'bonus': return { coins: 2, uscore: 1, lootChance: 1 };
		case 'invasion': return { lots: [], waves: 3, perWave: 5, interval: 90, radius: 25, center: 'spawn' };
		default: return { effectId: 0, effectType: '', interval: 10 };
		}
	}

	function field(i, path, label, value, attrs, help) {
		var id = 'evp-' + i + '-' + path.replace(/\./g, '-');
		return '<div class="mb-2"><label class="form-label small" for="' + id + '">' + label + '</label>' +
			'<input class="form-control form-control-sm" id="' + id + '" data-p="' + i + '" data-f="' + path + '" value="' + esc(value === undefined || value === null ? '' : value) + '" ' + (attrs || '') + '>' +
			(help ? '<div class="form-text">' + help + '</div>' : '') + '</div>';
	}

	function zonesField(i, path, selected, help) {
		var id = 'evp-' + i + '-zones';
		return '<div class="mb-2"><label class="form-label small" for="' + id + '">Zones</label>' +
			'<select multiple class="form-select form-select-sm ev-zones" id="' + id + '" data-p="' + i + '" data-f="' + path + '" data-zones>' + data.zones.map(function (z) {
				return '<option value="' + z.id + '"' + ((selected || []).indexOf(z.id) !== -1 ? ' selected' : '') + '>' + esc(z.id + ' · ' + z.name) + '</option>';
			}).join('') + '</select><div class="form-text">' + help + ' Ctrl-click for several.</div></div>';
	}

	function featureBody(p, i) {
		var inZones = data.features.filter(function (f) { return f.zones.length; }), others = data.features.filter(function (f) { return !f.zones.length; });
		function option(f) { return '<option value="' + esc(f.name) + '"' + (f.name === p.config.feature ? ' selected' : '') + '>' + esc(f.name) + (f.unlocked ? ' (already on)' : '') + '</option>'; }
		var known = !p.config.feature || feature(p.config.feature);
		var f = feature(p.config.feature), info = [];
		if (f) {
			if (f.description) info.push(esc(f.description));
			if (f.version) info.push('FeatureGating: from client version ' + esc(f.version) + (f.unlocked ? ' <span class="text-warning">(already on at ' + esc(data.clientVersion) + ', so it changes nothing in the zones)</span>' : ''));
			info.push(f.zones.length ? 'Objects in: ' + f.zones.map(function (z) { return esc(z.name) + ' (' + esc(z.objects) + ')'; }).join(', ') +
				'. Worlds of these zones that are running when it starts or ends need a restart.' : 'No zone has objects gated on it; it only reaches the client at login.');
		}
		return '<label class="form-label small" for="evp-' + i + '-feature">Feature</label><select class="form-select form-select-sm" id="evp-' + i + '-feature" data-p="' + i + '" data-f="feature" data-rerender>' +
			'<option value="">Pick a feature</option>' + (known ? '' : '<option value="' + esc(p.config.feature) + '" selected>' + esc(p.config.feature) + '</option>') +
			(inZones.length ? '<optgroup label="Used by objects in zones">' + inZones.map(option).join('') + '</optgroup>' : '') +
			(others.length ? '<optgroup label="Other features (FeatureGating)">' + others.map(option).join('') + '</optgroup>' : '') + '</select>' +
			'<div class="small mt-1">' + info.map(function (x) { return '<div>' + x + '</div>'; }).join('') + '</div>' +
			'<div class="form-text">It goes in a free <code>event_1</code>&ndash;<code>event_8</code> setting while the event is on; players get it at their next login.</div>';
	}

	function vanityFiles() { return pickers ? pickers.files : []; }

	function vanityBody(p, i) {
		if (!pickers) { loadPickers(); return '<p class="small text-body-secondary mb-0">Loading the vanity files&hellip;</p>'; }
		var c = p.config, switches = c.fileSwitches || {};
		var fileOptions = function (selected) {
			return '<option value="">Pick a vanity file</option>' + vanityFiles().map(function (f) {
				return '<option value="' + esc(f.name) + '"' + (f.name === selected ? ' selected' : '') + '>' + esc(f.name) + ' (' + (f.loaded ? 'loaded' : 'not loaded') + ', ' + esc(f.npcs) + ' NPCs)</option>';
			}).join('') + (selected && !vanityFiles().some(function (f) { return f.name === selected; }) ? '<option value="' + esc(selected) + '" selected>' + esc(selected) + ' (missing)</option>' : '');
		};
		var removals = Array.isArray(c.removals) ? c.removals : lines(c.removals || '');
		return '<div class="mb-2"><div class="form-label small mb-1">File switches <span class="text-body-secondary">(like root.xml\'s switches, while the event is on)</span></div>' +
			Object.keys(switches).map(function (name) {
				return '<div class="ev-switch"><select class="form-select form-select-sm" data-p="' + i + '" data-switch-file="' + esc(name) + '" aria-label="Vanity file">' + fileOptions(name) + '</select>' +
					'<select class="form-select form-select-sm w-auto" data-p="' + i + '" data-switch-on="' + esc(name) + '" aria-label="On or off">' +
					'<option value="1"' + (switches[name] ? ' selected' : '') + '>on</option><option value="0"' + (switches[name] ? '' : ' selected') + '>off</option></select>' +
					'<button type="button" class="btn btn-sm btn-outline-danger" data-p="' + i + '" data-switch-remove="' + esc(name) + '" aria-label="Remove the switch for ' + esc(name) + '">&times;</button></div>';
			}).join('') +
			'<button type="button" class="btn btn-sm btn-outline-secondary" data-p="' + i + '" data-switch-add>Add file switch</button>' +
			'<div class="form-text">On: the file (and what it includes) loads even if nothing switches it on. Off: it doesn\'t load, nor what only it includes.</div></div>' +
			'<div class="mb-2"><label class="form-label small" for="evp-' + i + '-file">Overlay file <span class="text-body-secondary">(optional)</span></label>' +
			'<div class="input-group input-group-sm"><input class="form-control font-monospace" id="evp-' + i + '-file" data-p="' + i + '" data-f="file" list="evVanityFiles" value="' + esc(c.file || '') + '" placeholder="e.g. halloween-look.xml" spellcheck="false" autocomplete="off">' +
			'<a class="btn btn-outline-secondary" href="/vanity' + (c.file ? '#file:' + encodeURIComponent(c.file) : '') + '" target="_blank" rel="noopener">Edit its NPCs</a></div>' +
			'<div class="form-text">Its NPCs are laid over the others: one with the same name as a vanity NPC replaces it (all of its locations). A new name makes an empty file when you save; root.xml shouldn\'t load it.</div></div>' +
			'<div class="mb-1"><label class="form-label small" for="evp-' + i + '-removals">NPCs to take out <span class="text-body-secondary">(one name per line)</span></label>' +
			'<textarea class="form-control form-control-sm font-monospace" id="evp-' + i + '-removals" data-p="' + i + '" data-f="removals" data-lines rows="2" spellcheck="false">' + esc(removals.join('\n')) + '</textarea>' +
			'<select class="form-select form-select-sm mt-1" data-p="' + i + '" data-removal-pick aria-label="Add an NPC to take out"><option value="">Add an NPC from the vanity files&hellip;</option>' +
			pickers.npcs.map(function (n) { return '<option value="' + esc(n.name) + '">' + esc(n.name + ' (' + n.files.join(', ') + ')') + '</option>'; }).join('') + '</select></div>';
	}

	function liveConfigBody(p, i) {
		var c = p.config.config || {}, type = p.config.type;
		switch (type) {
		case 'treasure_hunt':
			return '<div class="mb-2"><label class="form-label small" for="evp-' + i + '-lot">Treasure</label><input class="form-control form-control-sm" id="evp-' + i + '-lot" data-search="treasure" data-p="' + i + '" data-f="config.lot" data-value="' + esc(c.lot || '') + '" value="' + esc(c.lot ? 'LOT ' + c.lot : '') + '" placeholder="Search objects"></div>' +
				'<div class="row g-2">' + ['count|Treasures|1', 'coins|Coins per find|0', 'radius|Pickup distance|2'].map(function (x) {
					var bits = x.split('|');
					return '<div class="col-4">' + field(i, 'config.' + bits[0], bits[1], c[bits[0]], 'type="number" min="' + bits[2] + '" step="any"') + '</div>';
				}).join('') + '</div>' +
				'<div class="row g-2"><div class="col-8"><label class="form-label small" for="evp-' + i + '-item">Item reward <span class="text-body-secondary">(optional)</span></label><input class="form-control form-control-sm" id="evp-' + i + '-item" data-search="item" data-p="' + i + '" data-f="config.itemLot" data-value="' + esc(c.itemLot || '') + '" value="' + esc(c.itemLot ? 'LOT ' + c.itemLot : '') + '" placeholder="Search items"></div>' +
				'<div class="col-4">' + field(i, 'config.itemCount', 'Count', c.itemCount, 'type="number" min="1" max="999"') + '</div></div>';
		case 'bonus':
			return '<div class="row g-2">' + ['coins|Coins ×', 'uscore|U-score ×', 'lootChance|Loot chance ×'].map(function (x) {
				var bits = x.split('|');
				return '<div class="col-4">' + field(i, 'config.' + bits[0], bits[1], c[bits[0]], 'type="number" min="1" step="0.1"') + '</div>';
			}).join('') + '</div>';
		case 'invasion':
			return '<div class="mb-2"><div class="form-label small mb-1">Enemies</div><div class="ev-chips">' + (c.lots || []).map(function (lot, n) {
				return '<span class="badge text-bg-secondary">LOT ' + esc(lot) + ' <button type="button" class="btn-close btn-close-white" data-p="' + i + '" data-lot-remove="' + n + '" aria-label="Remove"></button></span>';
			}).join('') + '</div><input class="form-control form-control-sm" data-search="enemy" data-p="' + i + '" data-f="config.lots" data-add placeholder="Search enemies to add" aria-label="Add an enemy"></div>' +
				'<div class="row g-2">' + ['waves|Waves|1', 'perWave|Per wave|1', 'interval|Seconds apart|15', 'radius|Radius|5'].map(function (x) {
					var bits = x.split('|');
					return '<div class="col-3">' + field(i, 'config.' + bits[0], bits[1], c[bits[0]], 'type="number" min="' + bits[2] + '"') + '</div>';
				}).join('') + '</div>' +
				'<label class="form-label small" for="evp-' + i + '-center">They attack</label><select class="form-select form-select-sm" id="evp-' + i + '-center" data-p="' + i + '" data-f="config.center">' +
				'<option value="spawn"' + (c.center !== 'players' ? ' selected' : '') + '>the spawn point</option><option value="players"' + (c.center === 'players' ? ' selected' : '') + '>the players</option></select>';
		default:
			return '<div class="row g-2"><div class="col-8"><label class="form-label small" for="evp-' + i + '-effect">Effect</label><input class="form-control form-control-sm" id="evp-' + i + '-effect" data-search="effect" data-p="' + i + '" data-f="config.effectId" data-value="' + esc(c.effectId || '') + '" value="' + esc(c.effectId ? c.effectId + ' ' + (c.effectType || '') : '') + '" placeholder="Search effects"></div>' +
				'<div class="col-4">' + field(i, 'config.interval', 'Every (seconds)', c.interval, 'type="number" min="3" max="600"') + '</div></div>';
		}
	}

	function liveBody(p, i) {
		var c = p.config;
		var copy = recentLive && recentLive.length ? '<select class="form-select form-select-sm mb-2" data-p="' + i + '" data-live-copy aria-label="Copy a live event"><option value="">Copy the settings of a live event&hellip;</option>' +
			recentLive.map(function (e, n) { return '<option value="' + n + '">' + esc(e.title + ' (' + e.typeLabel + ', ' + e.zoneNames + ')') + '</option>'; }).join('') + '</select>' : '';
		if (!recentLive) loadRecentLive();
		return copy + '<div class="row g-2"><div class="col-sm-5"><label class="form-label small" for="evp-' + i + '-type">Kind</label><select class="form-select form-select-sm" id="evp-' + i + '-type" data-p="' + i + '" data-f="type" data-live-type>' +
			LIVE_TYPES.map(function (t) { return '<option value="' + t[0] + '"' + (t[0] === c.type ? ' selected' : '') + '>' + t[1] + '</option>'; }).join('') + '</select></div>' +
			'<div class="col-sm-7">' + field(i, 'title', 'Title', c.title, 'maxlength="100"') + '</div></div>' +
			field(i, 'message', 'Said when it starts <span class="text-body-secondary">(optional)</span>', c.message, 'maxlength="300"') +
			zonesField(i, 'zones', c.zones, c.type === 'bonus' ? 'None: every world.' : 'Where it runs.') +
			field(i, 'instance', 'Instance', c.instance, 'type="number" min="-1"', '-1: every instance of those zones.') +
			liveConfigBody(p, i) +
			'<div class="form-text">It starts when the event does and ends when it ends (a live event runs at most 7 days).</div>';
	}

	function announcementBody(p, i) {
		var c = p.config;
		return field(i, 'title', 'Title <span class="text-body-secondary">(optional)</span>', c.title, 'maxlength="100"') +
			'<div class="mb-2"><label class="form-label small" for="evp-' + i + '-message">Message</label><textarea class="form-control form-control-sm" id="evp-' + i + '-message" data-p="' + i + '" data-f="message" rows="2" maxlength="1000">' + esc(c.message) + '</textarea></div>' +
			'<div class="form-check mb-2"><input class="form-check-input" type="checkbox" id="evp-' + i + '-atStart" data-p="' + i + '" data-f="atStart"' + (c.atStart !== false ? ' checked' : '') + '><label class="form-check-label small" for="evp-' + i + '-atStart">Say it when the event starts</label></div>' +
			field(i, 'repeat', 'Repeat while it is on <span class="text-body-secondary">(optional)</span>', c.repeat, 'placeholder="@every 2h or 0 */3 * * *" spellcheck="false"', 'A schedule as on the Scheduled Announcements page (cron in UTC, or @every). At most once a minute.') +
			field(i, 'endMessage', 'Say when it ends <span class="text-body-secondary">(optional)</span>', c.endMessage, 'maxlength="1000"') +
			zonesField(i, 'zones', c.zones, 'None: every world.');
	}

	function restartBody(p, i) {
		var c = p.config;
		return '<div class="row g-2"><div class="col-sm-6"><label class="form-label small" for="evp-' + i + '-when">Restart</label><select class="form-select form-select-sm" id="evp-' + i + '-when" data-p="' + i + '" data-f="when">' +
			'<option value="start"' + (c.when === 'start' ? ' selected' : '') + '>when the event starts</option><option value="end"' + (c.when !== 'start' ? ' selected' : '') + '>when the event ends</option></select></div>' +
			'<div class="col-sm-6">' + field(i, 'minutes', 'Warn players this many minutes before', c.minutes, 'type="number" min="1" max="1440"') + '</div></div>' +
			field(i, 'reason', 'Reason <span class="text-body-secondary">(shown to players)</span>', c.reason, 'maxlength="300"') +
			'<div class="form-text">Needs a process supervisor to start the server again. A restart already scheduled is left as it is.</div>';
	}

	function partBody(p, i) {
		switch (p.kind) {
		case 'feature': return featureBody(p, i);
		case 'vanity': return vanityBody(p, i);
		case 'live_event': return liveBody(p, i);
		case 'announcement': return announcementBody(p, i);
		case 'restart': return restartBody(p, i);
		}
		return '';
	}

	function renderParts() {
		get('evParts').innerHTML = (parts.map(function (p, i) {
			var may = allowed(p.kind);
			return '<section class="ev-part" data-part="' + i + '"><div class="ev-part-head"><strong>' + esc(KIND_LABELS[p.kind] || p.kind) + '</strong>' +
				(p.applied !== undefined && editing ? (p.applied ? fmt.badge('on', 'success') : fmt.badge('off', 'secondary')) : '') +
				(p.status && editing ? '<span class="small text-body-secondary text-truncate" title="' + esc(p.status) + '">' + esc(p.status) + '</span>' : '') +
				'<button type="button" class="btn btn-sm btn-outline-danger ms-auto" data-remove-part="' + i + '"' + (may ? '' : ' disabled') + ' aria-label="Remove this part">&times;</button></div>' +
				(may ? partBody(p, i) : '<p class="small text-body-secondary mb-0">Needs ' + esc(permissionOf(p.kind)) + ' to change: ' + partSummary(p) + '</p>') + '</section>';
		}).join('') || '<p class="small text-body-secondary">Add what the event switches on.</p>') +
			'<datalist id="evVanityFiles">' + vanityFiles().map(function (f) { return '<option value="' + esc(f.name) + '">'; }).join('') + '</datalist>';
		attachSearches();
	}

	// Object and effect pickers of live event parts, as on the Live Events page
	function attachSearches() {
		Array.prototype.forEach.call(get('evParts').querySelectorAll('[data-search]'), function (input) {
			var kind = input.dataset.search, i = Number(input.dataset.p), path = input.dataset.f;
			var value = input.dataset.value, label = input.value;
			SearchSelect(input, {
				value: value, label: label,
				search: function (q) {
					if (kind === 'effect') return api.get('/api/live-events/effects?q=' + encodeURIComponent(q)).then(function (d) {
						return (d.effects || []).map(function (e) { return { value: e.id + '|' + e.type, label: e.id + ' ' + e.type, detail: e.name }; });
					});
					return api.get('/api/live-events/objects?kind=' + kind + '&q=' + encodeURIComponent(q)).then(function (d) {
						return (d.objects || []).map(function (o) { return { value: String(o.lot), label: o.name + ' (LOT ' + o.lot + ')', detail: o.type }; });
					});
				},
				onPick: function (item) {
					var c = parts[i].config.config;
					if (kind === 'effect') { var bits = item.value.split('|'); c.effectId = Number(bits[0]); c.effectType = bits.slice(1).join('|'); return; }
					if (input.hasAttribute('data-add')) {
						if ((c.lots || []).indexOf(Number(item.value)) === -1) (c.lots = c.lots || []).push(Number(item.value));
						renderParts();
						return;
					}
					c[path.split('.')[1]] = Number(item.value);
				}
			});
		});
	}

	function loadPickers() {
		if (pickers || loadPickers.busy || !DASH.can('vanity_manage')) return;
		loadPickers.busy = true;
		api.get('/api/vanity/pickers').then(function (d) {
			loadPickers.busy = false;
			if (!d.success) return;
			pickers = d;
			renderParts();
		});
	}

	function loadRecentLive() {
		if (recentLive || !DASH.can('live_events_manage')) return;
		recentLive = [];
		api.get('/api/live-events').then(function (d) {
			if (!d.success) return;
			recentLive = d.events.slice(0, 20);
			if (parts.some(function (p) { return p.kind === 'live_event'; })) renderParts();
		});
	}

	function setPath(obj, path, value) {
		var keys = path.split('.'), target = obj;
		for (var k = 0; k < keys.length - 1; k++) target = target[keys[k]] = target[keys[k]] || {};
		target[keys[keys.length - 1]] = value;
	}

	function partChanged(e) {
		var t = e.target, i = Number(t.dataset.p);
		if (isNaN(i) || !parts[i]) return;
		var p = parts[i];
		if (t.dataset.f && !t.hasAttribute('data-search')) {
			var value = t.type === 'checkbox' ? t.checked : t.type === 'number' ? (t.value === '' ? '' : Number(t.value)) :
				t.hasAttribute('data-zones') ? Array.prototype.map.call(t.selectedOptions, function (o) { return Number(o.value); }) :
				t.hasAttribute('data-lines') ? lines(t.value) : t.value;
			setPath(p.config, t.dataset.f, value);
			if (t.hasAttribute('data-live-type')) { p.config.config = liveDefaults(t.value); renderParts(); }
			else if (t.hasAttribute('data-rerender') && e.type === 'change') renderParts();
			return;
		}
		if (t.dataset.switchFile !== undefined && e.type === 'change') {
			var switches = p.config.fileSwitches, old = t.dataset.switchFile, on = switches[old];
			delete switches[old];
			if (t.value) switches[t.value] = on;
			renderParts();
		} else if (t.dataset.switchOn !== undefined) {
			p.config.fileSwitches[t.dataset.switchOn] = t.value === '1';
		} else if (t.hasAttribute('data-removal-pick') && t.value) {
			var list = Array.isArray(p.config.removals) ? p.config.removals : lines(p.config.removals || '');
			if (list.indexOf(t.value) === -1) list.push(t.value);
			p.config.removals = list;
			renderParts();
		} else if (t.hasAttribute('data-live-copy') && t.value !== '') {
			var from = recentLive[Number(t.value)];
			p.config = { type: from.type, title: from.title, message: from.message, zones: from.zones.slice(), instance: from.instanceId, config: JSON.parse(JSON.stringify(from.config)) };
			renderParts();
		}
	}

	// ---------------------------------------------------------------- the editor

	function open(e, day) {
		editing = e || null;
		var may = !e || mayChange(e);
		get('evModalTitle').textContent = e ? (may ? 'Change ' : '') + e.name : 'New event';
		get('evName').value = e ? e.name : '';
		get('evNote').value = e ? e.note : '';
		get('evPriority').value = e ? e.priority : 0;
		get('evMode').innerHTML = data.modes.map(function (m) { return '<option value="' + m.value + '">' + esc(m.name) + '</option>'; }).join('');
		get('evMode').value = String(e ? e.mode : 1);
		var start = day ? Math.floor(day.getTime() / 1000) : Math.ceil(Date.now() / 3600000) * 3600;
		get('evStarts').value = toInput(e && e.once ? e.startsAt : start);
		get('evEnds').value = toInput(e && e.once ? e.endsAt : start + 86400);
		get('evJsonMode').checked = false;
		get('evJson').classList.add('d-none');
		get('evBuilder').classList.remove('d-none');
		loadBuilder(e && e.schedule ? e.schedule : { utcOffset: browserOffset(), match: 'any', rules: [newRule('yearly')] });
		parts = e ? e.parts.map(function (p) { return { kind: p.kind, config: JSON.parse(JSON.stringify(p.config)), applied: p.applied, status: p.status }; }) : [];
		renderParts();
		setWhen(!!(e && !e.once));
		get('evForm').querySelector('[type=submit]').disabled = !may;
		modal.show();
	}

	function eventBody(e) {
		return { name: e.name, note: e.note, mode: e.mode, schedule: e.once ? null : e.schedule, startsAt: e.once ? e.startsAt : 0, endsAt: e.once ? e.endsAt : 0,
			priority: e.priority, parts: e.parts.map(function (p) { return { kind: p.kind, config: p.config }; }) };
	}

	// ---------------------------------------------------------------- wiring

	get('evAdd').addEventListener('click', function () { open(null); });
	get('evFilter').addEventListener('input', renderList);
	get('evShowOver').addEventListener('change', renderList);
	get('evOnce').addEventListener('change', function () { setWhen(false); });
	get('evRepeat').addEventListener('change', function () { setWhen(true); });
	get('evStarts').addEventListener('change', schedulePreview);
	get('evEnds').addEventListener('change', schedulePreview);
	get('evMatch').addEventListener('change', schedulePreview);
	get('evOffset').addEventListener('change', function () { offsetInfo(); schedulePreview(); });
	get('evJson').addEventListener('input', schedulePreview);

	get('evAddRule').addEventListener('click', function () { rules.push(newRule(get('evNewRule').value)); renderRules(); schedulePreview(); });
	get('evRules').addEventListener('click', function (e) {
		var remove = e.target.closest('[data-remove-rule]');
		if (!remove) return;
		rules.splice(Number(remove.dataset.removeRule), 1);
		renderRules();
		schedulePreview();
	});

	function ruleChanged(e) {
		var t = e.target, i = Number(t.dataset.i);
		if (isNaN(i) || !rules[i]) return;
		var r = rules[i];
		if (t.dataset.field) {
			r[t.dataset.field] = t.type === 'number' ? Number(t.value) : t.value;
		} else if (t.dataset.day) {
			r.days = DAYS.filter(function (d) { return get('ev-day-' + i + '-' + d).checked; });
		} else if (t.hasAttribute('data-not')) {
			if (t.checked) r.not = true; else delete r.not;
		} else if (t.hasAttribute('data-window')) {
			if (t.value === 'hours') { delete r.days; r.hours = 12; } else { delete r.hours; r.days = 0; }
			renderRules();
		} else if (t.hasAttribute('data-group')) {
			try {
				var group = JSON.parse(t.value);
				r.match = group.match;
				r.rules = group.rules;
				t.classList.remove('is-invalid');
			} catch (err) {
				t.classList.add('is-invalid');
				return;
			}
		}
		schedulePreview();
	}
	get('evRules').addEventListener('input', ruleChanged);
	get('evRules').addEventListener('change', function (e) { if (e.target.tagName === 'SELECT' || e.target.type === 'checkbox') ruleChanged(e); });

	get('evJsonMode').addEventListener('change', function () {
		if (this.checked) {
			get('evJson').value = JSON.stringify(builderSchedule(), null, 2);
		} else {
			var schedule = currentSchedule(false);
			if (!schedule) { this.checked = true; return; }
			loadBuilder(schedule);
		}
		get('evJson').classList.toggle('d-none', !this.checked);
		get('evBuilder').classList.toggle('d-none', this.checked);
		schedulePreview();
	});

	get('evAddPart').addEventListener('click', function () {
		var kind = get('evNewPart').value;
		if (!allowed(kind)) return;
		parts.push({ kind: kind, config: defaultConfig(kind) });
		renderParts();
		var last = get('evParts').querySelector('[data-part="' + (parts.length - 1) + '"]');
		if (last) last.scrollIntoView({ block: 'nearest' });
	});
	get('evParts').addEventListener('input', partChanged);
	get('evParts').addEventListener('change', partChanged);
	get('evParts').addEventListener('click', function (e) {
		var t = e.target.closest('[data-remove-part],[data-switch-add],[data-switch-remove],[data-lot-remove]');
		if (!t) return;
		var i = Number(t.dataset.p !== undefined ? t.dataset.p : t.dataset.removePart);
		if (t.dataset.removePart !== undefined) parts.splice(Number(t.dataset.removePart), 1);
		else if (t.hasAttribute('data-switch-add')) {
			var free = vanityFiles().filter(function (f) { return !(f.name in parts[i].config.fileSwitches); })[0];
			if (!free) { toast('Every vanity file has a switch already', 'info'); return; }
			parts[i].config.fileSwitches[free.name] = !free.loaded;
		} else if (t.dataset.switchRemove !== undefined) delete parts[i].config.fileSwitches[t.dataset.switchRemove];
		else if (t.dataset.lotRemove !== undefined) parts[i].config.config.lots.splice(Number(t.dataset.lotRemove), 1);
		renderParts();
	});

	get('evForm').addEventListener('submit', function (e) {
		e.preventDefault();
		var schedule = currentSchedule(false);
		if (schedule === undefined) return;
		var body = {
			name: get('evName').value.trim(),
			note: get('evNote').value.trim(),
			priority: Number(get('evPriority').value) || 0,
			mode: Number(get('evMode').value),
			schedule: schedule,
			startsAt: schedule ? 0 : fromInput(get('evStarts').value),
			endsAt: schedule ? 0 : fromInput(get('evEnds').value),
			parts: parts.map(function (p) { return { kind: p.kind, config: p.config }; })
		};
		api.action(editing ? '/api/events/' + editing.id : '/api/events', body).then(function (d) {
			toast(d.message || 'Saved', 'success');
			modal.hide();
			load();
		}).catch(function () {});
	});

	get('evRows').addEventListener('change', function (e) {
		var select = e.target.closest('[data-mode]');
		if (!select) return;
		api.action('/api/events/' + select.dataset.mode + '/mode', { mode: Number(select.value) }).then(function (d) { toast(d.message, 'success'); load(); }, load);
	});

	get('evRows').addEventListener('click', function (e) {
		var el = e.target.closest('[data-edit],[data-export],[data-cancel],[data-delete],[data-shutdown]');
		if (!el) return;
		if (el.dataset.edit) return open(byId(el.dataset.edit));
		if (el.dataset.export) {
			var text = JSON.stringify(eventBody(byId(el.dataset.export)), null, 2);
			return navigator.clipboard.writeText(text).then(function () { toast('Copied the event as JSON', 'success'); }, function () { window.prompt('Copy the event:', text); });
		}
		if (el.dataset.cancel && confirm('Switch this event off now? Whatever it switched on is ended.')) {
			return api.action('/api/events/' + el.dataset.cancel + '/cancel', {}, 'Cancelled').then(load, load);
		}
		if (el.dataset.delete && confirm('Delete this event? Whatever it switched on is ended first; vanity files it used stay in the vanity folder.')) {
			return api.action('/api/events/' + el.dataset.delete + '/delete', {}, 'Deleted').then(load, load);
		}
		if (el.dataset.shutdown && confirm('Shut this world down? Its players are saved and disconnected; a fresh instance starts when someone goes there.')) {
			var bits = el.dataset.shutdown.split('/');
			api.action('/api/worlds/shutdown', { zone: +bits[0], instance: +bits[1] }, 'Shutting it down');
		}
	});

	get('calPrev').addEventListener('click', function () { cal.shown.setMonth(cal.shown.getMonth() - 1); renderCalendar(); });
	get('calNext').addEventListener('click', function () { cal.shown.setMonth(cal.shown.getMonth() + 1); renderCalendar(); });
	get('calendar').addEventListener('click', function (e) {
		var chip = e.target.closest('[data-open]');
		if (chip && byId(chip.dataset.open)) open(byId(chip.dataset.open));
	});

	get('evImport').addEventListener('click', function () { get('evImportText').value = ''; importModal.show(); });
	get('evImportForm').addEventListener('submit', function (e) {
		e.preventDefault();
		var list;
		try { list = JSON.parse(get('evImportText').value); } catch (err) { toast('Not valid JSON: ' + err.message, 'danger'); return; }
		if (!Array.isArray(list)) list = [list];
		var done = 0;
		// One at a time, so the first bad one stops the rest with its error
		list.reduce(function (chain, item) {
			return chain.then(function () { return api.action('/api/events', item).then(function () { done++; }); });
		}, Promise.resolve()).then(function () {
			toast('Imported ' + done + ' event(s)', 'success');
			importModal.hide();
		}).catch(function () {
			if (done) toast('Imported ' + done + ' before that one', 'info');
		}).then(load);
	});

	// Time zone choices, every quarter hour from UTC-12 to UTC+14
	get('evOffset').innerHTML = (function () {
		var html = '', mine = browserOffset();
		for (var m = -720; m <= 840; m += 15) html += '<option value="' + m + '">' + offsetLabel(m) + (m === 0 ? ' (server time)' : '') + (m === mine ? ' (your browser)' : '') + '</option>';
		return html;
	})();

	if (window.Live) {
		Live.on('scheduled_events', Live.throttle(load, 500));
		Live.on('live_events', Live.throttle(function () { cal.range = ''; renderCalendar(); }, 1000));
	}
	setInterval(load, 60000); // on and off as time passes, and the running worlds
	load().then(function () {
		// #event:ID opens that event, #new a new one, #new:KIND a new one with that part
		var hash = decodeURIComponent(location.hash.slice(1));
		if (hash === 'new') open(null);
		else if (hash.indexOf('new:') === 0 && allowed(hash.slice(4))) {
			open(null);
			parts.push({ kind: hash.slice(4), config: defaultConfig(hash.slice(4)) });
			renderParts();
		} else if (hash.indexOf('event:') === 0 && byId(hash.slice(6))) open(byId(hash.slice(6)));
	});
})();
