/**
 * The Vanity page, Files & NPCs tab (the Preview tab is vanity-preview.js).
 *
 * - Left: the vanity files as the world reads them: root.xml, then the files each one includes (<file name=".." enabled="1"/>),
 *   any file may include others. A switch per include; files nothing includes are listed apart. Badges say which
 *   scheduled events use a file (as their overlay file, or switching it on or off) and what the worlds load now with
 *   the events that are on. Then the events with vanity changes, and the plaque texts.
 * - Right: the chosen file's includes and NPCs, edited in place. Nothing is sent until Save, which sends the whole file;
 *   the server checks every NPC the way the world server reads it and keeps the old file as .bak.
 * - Inputs keep what was typed (strings); numbers are made when saving, so half-typed values don't jump around.
 * - Rotation is shown as a facing in degrees (a turn about the up axis), with the raw quaternion behind "Advanced".
 * - An NPC that an event replaces or takes out has a badge linking to the event.
 * - The hash is #file:NAME or #text:NAME on this tab, #preview on the other.
 */
(function () {
	'use strict';

	var RANDOM_KEY = 'useLocationsAsRandomSpawnPoint=7:1';
	var DEFAULT_LOT = 2279;
	var DEFAULT_ZONE = 1200;
	var ROOT = 'root.xml';
	var FILE_NAME = /^[A-Za-z0-9_-]{1,60}\.xml$/;

	var el = {
		page: document.getElementById('vanityPage'),
		folder: document.getElementById('vanityFolder'),
		files: document.getElementById('vanityFiles'),
		texts: document.getElementById('vanityTexts'),
		events: document.getElementById('vanityEvents'),
		tabFiles: document.getElementById('vanityTabFiles'),
		main: document.getElementById('vanityMain'),
		disabled: document.getElementById('vanityDisabled'),
		rootError: document.getElementById('vanityRootError'),
		reload: document.getElementById('vanityReload'),
		newFile: document.getElementById('vanityNewFile'),
		saveBar: document.getElementById('vanitySaveBar'),
		saveLabel: document.getElementById('vanitySaveLabel'),
		save: document.getElementById('vanitySave'),
		discard: document.getElementById('vanityDiscard')
	};

	var state = {
		files: [], texts: [], zones: [], zoneNames: {}, events: [],
		npcEvents: {},     // NPC name -> the events that replace or take it out
		selected: null,    // { kind: 'file' | 'text', name }
		objects: null,     // the chosen file's NPCs as edited (see fromServer)
		baseline: '',      // what the file held when loaded, serialized, to tell whether anything changed
		includes: null,    // the chosen file's <file> entries as edited: [{name, enabled}]
		includesBaseline: '',
		loadError: '',
		itemNames: {},     // lot -> item name
		open: {},          // NPC id -> card expanded
		raw: {},           // NPC id -> showing the raw quaternion
		filter: '',
		text: null,        // { name, where, value, baseline } for a plaque text
		saving: false,
		nextId: 1
	};

	// ---------------------------------------------------------------- model helpers

	// Floats arrive as doubles (287.6 becomes 287.6000061035156): show them the way they were written
	function tidy(n) { return n === undefined || n === null ? '' : String(Number(Number(n).toPrecision(7))); }
	function num(v) { return String(v).trim() === '' ? NaN : Number(v); }
	function lines(text) { return String(text).split('\n').map(function (l) { return l.trim(); }).filter(Boolean); }
	function plural(n, word) { return n + ' ' + word + (n === 1 ? '' : 's'); }

	function fromServer(o) {
		var config = (o.config || []).slice();
		var random = config.indexOf(RANDOM_KEY) !== -1;
		return {
			id: state.nextId++,
			name: o.name || '',
			lot: String(o.lot || ''),
			equipment: (o.equipment || []).slice(),
			phrases: (o.phrases || []).join('\n'),
			config: config.filter(function (c) { return c !== RANDOM_KEY; }).join('\n'),
			random: random,
			locations: (o.locations || []).map(function (l) {
				return { zone: String(l.zone), x: tidy(l.x), y: tidy(l.y), z: tidy(l.z), rw: tidy(l.rw), rx: tidy(l.rx), ry: tidy(l.ry), rz: tidy(l.rz),
					chance: tidy(l.chance), scale: tidy(l.scale) };
			})
		};
	}

	// What the server takes: {name, lot, equipment, phrases, config, locations}
	function toServer(o) {
		var config = lines(o.config).filter(function (c) { return c !== RANDOM_KEY; });
		if (o.random) config.unshift(RANDOM_KEY);
		return {
			name: o.name.trim(), lot: num(o.lot), equipment: o.equipment.slice(), phrases: lines(o.phrases), config: config,
			locations: o.locations.map(function (l) {
				var out = { zone: num(l.zone), x: num(l.x), y: num(l.y), z: num(l.z), rw: num(l.rw), rx: num(l.rx), ry: num(l.ry), rz: num(l.rz) };
				if (String(l.chance).trim() !== '') out.chance = num(l.chance);
				if (String(l.scale).trim() !== '') out.scale = num(l.scale);
				return out;
			})
		};
	}

	function serialize() { return state.objects ? JSON.stringify(state.objects.map(toServer)) : ''; }

	function isDirty() {
		if (!state.selected) return false;
		if (state.selected.kind === 'text') return !!state.text && state.text.value !== state.text.baseline;
		return !!state.objects && (serialize() !== state.baseline || includesDirty());
	}

	function includesDirty() { return !!state.includes && JSON.stringify(state.includes) !== state.includesBaseline; }

	function byId(id) {
		id = Number(id);
		for (var i = 0; i < (state.objects || []).length; i++) if (state.objects[i].id === id) return state.objects[i];
		return null;
	}

	function newLocation(from) {
		return { zone: from ? from.zone : String(DEFAULT_ZONE), x: '0', y: '0', z: '0', rw: '1', rx: '0', ry: '0', rz: '0', chance: '', scale: '' };
	}

	// ---- rotation: a facing in degrees for turns about the up (Y) axis

	function yawOnly(l) { return Math.abs(Number(l.rx) || 0) < 1e-4 && Math.abs(Number(l.rz) || 0) < 1e-4; }

	function facing(l) {
		var rw = Number(l.rw), ry = Number(l.ry);
		if (!isFinite(rw) || !isFinite(ry) || !yawOnly(l)) return '';
		var deg = 2 * Math.atan2(ry, rw) * 180 / Math.PI;
		while (deg > 180) deg -= 360;
		while (deg <= -180) deg += 360;
		return String(Math.round(deg * 10) / 10);
	}

	function setFacing(l, degrees) {
		var half = Number(degrees) * Math.PI / 360;
		l.rw = tidy(Math.round(Math.cos(half) * 1e6) / 1e6);
		l.ry = tidy(Math.round(Math.sin(half) * 1e6) / 1e6);
		l.rx = '0';
		l.rz = '0';
	}

	// ---- checks, the same ones the server makes

	function fieldError(field, value) {
		var n = num(value);
		switch (field) {
		// Names are optional: the world spawns nameless objects too (demo.xml's plaque and trees)
		case 'name': return String(value).length > 100 ? 'Up to 100 characters' : '';
		case 'lot': return n > 0 && Math.floor(n) === n ? '' : 'A LOT above 0';
		case 'zone': return n > 0 ? '' : 'Pick a zone';
		case 'x': case 'y': case 'z': case 'rw': case 'rx': case 'ry': case 'rz': return isFinite(n) ? '' : 'A number';
		case 'facing': return String(value).trim() === '' || isFinite(n) ? '' : 'A number';
		case 'chance': return String(value).trim() === '' || (n >= 0 && n <= 1) ? '' : 'Between 0 and 1';
		case 'scale': return String(value).trim() === '' || n > 0 ? '' : 'Above 0';
		default: return '';
		}
	}

	function configError(text) {
		var bad = lines(text).filter(function (c) {
			var equals = c.indexOf('='), colon = c.indexOf(':');
			return equals <= 0 || colon < equals || !/^\s*[-+]?\d+\s*$/.test(c.slice(equals + 1, colon));
		})[0];
		return bad ? '"' + bad + '" should look like name=type:value' : '';
	}

	// The first problem with an NPC, or ''
	function npcError(o) {
		var who = '"' + (o.name.trim() || 'NPC without a name') + '": ';
		if (fieldError('name', o.name)) return who + 'the name is too long';
		if (fieldError('lot', o.lot)) return who + 'pick a LOT';
		var config = configError(o.config);
		if (config) return who + 'config ' + config;
		if (!o.locations.length) return who + 'add at least one location';
		for (var i = 0; i < o.locations.length; i++) {
			var l = o.locations[i];
			var fields = ['zone', 'x', 'y', 'z', 'rw', 'rx', 'ry', 'rz', 'chance', 'scale'];
			for (var f = 0; f < fields.length; f++) {
				var error = fieldError(fields[f], l[fields[f]]);
				if (error) return who + 'location ' + (i + 1) + ' ' + fields[f] + ': ' + error.toLowerCase();
			}
		}
		return '';
	}

	// ---- names

	function itemName(lot) { return state.itemNames[String(lot)] || ''; }

	function zoneLabel(id) { return id + (state.zoneNames[id] ? ' · ' + state.zoneNames[id] : ''); }

	function fetchNames(lots) {
		var missing = lots.map(String).filter(function (lot) { return /^\d+$/.test(lot) && !(lot in state.itemNames); });
		if (!missing.length) return Promise.resolve();
		return api.get('/api/vanity/names?lots=' + missing.join(',')).then(function (d) {
			if (!d.success) return;
			missing.forEach(function (lot) { state.itemNames[lot] = d.itemNames[lot] || ''; });
		});
	}

	// ---------------------------------------------------------------- rendering: the side lists

	// Why a file isn't loaded in game, or '' when it is
	function notLoadedWhy(f) {
		if (!f.exists) return 'The file is missing';
		if (f.error) return 'It can\'t be read: ' + f.error;
		if (f.loaded) return '';
		var by = f.includedBy || [];
		if (!by.length) return 'No file includes it';
		var loadedParents = by.filter(function (b) { var p = fileInfo(b.file); return p && p.loaded; });
		if (loadedParents.length) return 'Switched off in ' + loadedParents.map(function (b) { return b.file; }).join(', ');
		return 'Only included by files that aren\'t loaded (' + by.map(function (b) { return b.file; }).join(', ') + ')';
	}

	// What the events do with a file: its overlay file, or switching it on or off; filled in while the event is on
	var USES = { overlay: 'overlay of', on: 'on by', off: 'off by' };
	function eventBadges(list, labels) {
		return (list || []).map(function (u) {
			return '<a class="badge vanity-event-badge' + (u.on ? ' is-on' : '') + '" href="/events#event:' + u.id + '" title="' + esc('Scheduled event ' + u.name + (u.on ? ' (on now)' : ' (off now)')) + '">' +
				esc((labels[u.use] || u.use) + ' ' + u.name) + '</a> ';
		}).join('');
	}

	// What the worlds load now differs from what the files say: an event that is on switched it
	function nowBadge(f) {
		if (!f.exists || f.loadedNow === f.loaded || f.root) return '';
		return '<span class="badge ' + (f.loadedNow ? 'vanity-badge-loaded' : 'vanity-badge-off') + '" title="' + esc('Switched by ' + (f.switchedNow || 'an event') + ', which is on now') + '">' +
			(f.loadedNow ? 'on now' : 'off now') + '</span> ';
	}

	// In the narrow file list: one badge, the events in its tooltip
	function compactEventBadge(list) {
		if (!list || !list.length) return '';
		var title = list.map(function (u) { return (USES[u.use] || u.use) + ' ' + u.name + (u.on ? ' (on now)' : ''); }).join('; ');
		return '<span class="badge vanity-event-badge' + (list.some(function (u) { return u.on; }) ? ' is-on' : '') + '" title="' + esc('Scheduled events: ' + title) + '">' +
			list.length + (list.length === 1 ? ' event' : ' events') + '</span> ';
	}

	function fileBadges(f, compact) {
		if (!f) return '<span class="badge text-bg-warning">missing</span>';
		var why = notLoadedWhy(f);
		return (compact ? compactEventBadge(f.events) : eventBadges(f.events, USES)) + nowBadge(f) + (f.error ? '<span class="badge text-bg-danger" title="' + esc(f.error) + '">error</span> ' : '') +
			(!f.exists ? '<span class="badge text-bg-warning" title="' + esc(why) + '">missing</span>'
				: f.loaded ? '<span class="badge vanity-badge-loaded" title="The worlds load this file">loaded</span>'
				: '<span class="badge vanity-badge-off" title="' + esc(why) + '">not loaded</span>');
	}

	// One row of the tree; `edge` is the <file> entry in `parent` that brings it in (none for root.xml)
	function fileRow(name, parent, edge, depth, again) {
		var f = fileInfo(name);
		var active = state.selected && state.selected.kind === 'file' && state.selected.name === name;
		var exists = f && f.exists;
		var label = '<span class="text-truncate">' + esc(name) + '</span>' +
			'<span class="vanity-count">' + (exists && !f.error ? plural(f.npcs, 'NPC') + ' ' : '') + fileBadges(f, true) +
			(again ? ' <span title="Shown in full above">(see above)</span>' : '') + '</span>';
		var why = f ? notLoadedWhy(f) : 'The file is missing';
		return '<div class="list-group-item vanity-file' + (active ? ' active' : '') + (again ? ' is-repeat' : '') + (depth ? ' is-child' : '') + '" style="--depth:' + depth + '">' +
			(exists ? '<a href="#file:' + esc(name) + '" class="vanity-file-name" data-select-file="' + esc(name) + '" title="' + esc(why || 'Loaded in game') + '">' + label + '</a>'
				: '<div class="vanity-file-name" title="' + esc(why) + '">' + label + '</div>') +
			(parent ? '<div class="form-check form-switch mb-0" title="' + esc((edge.enabled ? 'On' : 'Off') + ' in ' + parent) + '">' +
				'<input class="form-check-input" type="checkbox" role="switch" data-enable="' + esc(name) + '" data-parent="' + esc(parent) + '"' + (edge.enabled ? ' checked' : '') +
				' aria-label="Include ' + esc(name) + ' from ' + esc(parent) + '"></div>' : '') +
			'</div>';
	}

	// root.xml, then what each file includes, depth first. A file included twice is shown under each parent, but its
	// own includes only the first time; a file including one of its own ancestors stops there.
	function treeRows() {
		var rows = [], shown = {};
		(function walk(name, parent, edge, depth, path) {
			var again = !!shown[name];
			rows.push(fileRow(name, parent, edge, depth, again && (fileInfo(name) || {}).includes && fileInfo(name).includes.length));
			if (again || path.indexOf(name) !== -1) return;
			shown[name] = true;
			var f = fileInfo(name);
			(f && f.includes || []).forEach(function (inc) {
				if (path.indexOf(inc.name) !== -1 || inc.name === name) {
					rows.push('<div class="list-group-item vanity-file small text-warning-emphasis" style="--depth:' + (depth + 1) + '">' + esc(inc.name) + ' &middot; loops back to a file above; the world loads each file once</div>');
					return;
				}
				walk(inc.name, name, inc, depth + 1, path.concat(name));
			});
		})(ROOT, null, null, 0, []);
		return { html: rows.join(''), shown: shown };
	}

	function renderSide() {
		var tree = treeRows();
		var loose = state.files.filter(function (f) { return !f.root && !tree.shown[f.name] && f.exists; });
		el.files.innerHTML = tree.html + (loose.length ? '<div class="list-group-item vanity-group-label">Not included</div>' + loose.map(function (f) {
			var active = state.selected && state.selected.kind === 'file' && state.selected.name === f.name;
			return '<div class="list-group-item vanity-file' + (active ? ' active' : '') + '" style="--depth:0">' +
				'<a href="#file:' + esc(f.name) + '" class="vanity-file-name" data-select-file="' + esc(f.name) + '"><span class="text-truncate">' + esc(f.name) + '</span>' +
				'<span class="vanity-count">' + (f.error ? '' : plural(f.npcs, 'NPC') + ' ') + fileBadges(f, true) + '</span></a>' +
				'<button type="button" class="btn btn-outline-secondary btn-sm" data-include-root="' + esc(f.name) + '" title="Add it to root.xml (switched off)">Include</button></div>';
		}).join('') : '');

		el.events.innerHTML = state.events.map(function (e) {
			return '<a href="/events#event:' + e.id + '" class="list-group-item list-group-item-action d-flex justify-content-between align-items-center gap-2">' +
				'<span class="text-truncate">' + esc(e.name) + '<span class="vanity-where d-block">' + esc(e.on ? (e.nextEnd ? 'on until ' + fmt.unix(e.nextEnd) : 'on') :
					e.nextStart ? 'next ' + fmt.unix(e.nextStart) : e.modeName) + ' &middot; priority ' + esc(e.priority) + '</span></span>' +
				(e.on ? '<span class="badge vanity-badge-loaded">on</span>' : '<span class="badge vanity-badge-off">off</span>') + '</a>';
		}).join('') || '<div class="list-group-item small text-body-secondary">No event changes the vanity NPCs. <a href="/events#new:vanity">Add one</a>.</div>';

		el.texts.innerHTML = state.texts.map(function (t) {
			var active = state.selected && state.selected.kind === 'text' && state.selected.name === t.name;
			return '<a href="#text:' + esc(t.name) + '" class="list-group-item list-group-item-action' + (active ? ' active' : '') + '" data-select-text="' + esc(t.name) + '">' +
				'<div class="fw-semibold">' + esc(t.name) + '</div><div class="vanity-where">' + esc(t.where) + '</div></a>';
		}).join('');
	}

	// ---------------------------------------------------------------- rendering: the chosen file

	function zoneOptions(selected) {
		var known = state.zones.some(function (z) { return String(z.id) === String(selected); });
		return (known || !selected ? '' : '<option value="' + esc(selected) + '" selected>' + esc(selected) + ' · unknown</option>') +
			state.zones.map(function (z) {
				return '<option value="' + z.id + '"' + (String(z.id) === String(selected) ? ' selected' : '') + '>' + esc(z.id + ' · ' + z.name) + '</option>';
			}).join('');
	}

	function numberField(field, label, value, extra) {
		var error = fieldError(field, value);
		return '<label class="loc-field loc-' + field + '"><span>' + label + '</span><input type="number" step="any" class="form-control form-control-sm' +
			(error ? ' is-invalid' : '') + '" data-loc-field="' + field + '" value="' + esc(value) + '"' + (extra || '') + '></label>';
	}

	function locationHtml(o, l, i) {
		var raw = state.raw[o.id] || !yawOnly(l);
		return '<div class="npc-loc" data-loc="' + i + '">' +
			'<label class="loc-field loc-zone"><span>Zone</span><select class="form-select form-select-sm" data-loc-field="zone">' + zoneOptions(l.zone) + '</select></label>' +
			numberField('x', 'X', l.x) + numberField('y', 'Y', l.y) + numberField('z', 'Z', l.z) +
			numberField('facing', 'Facing (°)', facing(l), ' step="1"' + (yawOnly(l) ? '' : ' placeholder="tilted" title="Tilted, not just turned: edit the quaternion"')) +
			(raw ? numberField('rw', 'rw', l.rw) + numberField('rx', 'rx', l.rx) + numberField('ry', 'ry', l.ry) + numberField('rz', 'rz', l.rz) : '') +
			numberField('chance', 'Chance', l.chance, ' min="0" max="1" placeholder="always" title="0 to 1: how likely the NPC appears here"') +
			numberField('scale', 'Scale', l.scale, ' min="0" placeholder="normal"') +
			'<button type="button" class="btn btn-outline-danger btn-sm loc-remove" data-action="remove-loc"' + (o.locations.length > 1 ? '' : ' disabled') +
			' aria-label="Remove location ' + (i + 1) + '" title="Remove this location">&times;</button></div>';
	}

	function summary(o) {
		var zones = [];
		o.locations.forEach(function (l) { if (zones.indexOf(l.zone) === -1) zones.push(l.zone); });
		var name = itemName(o.lot);
		return 'LOT ' + esc(o.lot || '?') + (name ? ' · ' + esc(name) : '') + ' · ' + plural(o.locations.length, 'location') +
			(zones.length ? ' in ' + esc(zones.map(zoneLabel).join(', ')) : '');
	}

	function headHtml(o) {
		var open = !!state.open[o.id], error = npcError(o);
		return '<button type="button" class="npc-toggle" data-action="toggle" aria-expanded="' + open + '">' +
			'<span class="npc-caret" aria-hidden="true"></span>' +
			'<span class="npc-heading"><span class="npc-title">' + (o.name.trim() ? esc(o.name) : '<em>No name</em>') + '</span>' +
			'<span class="npc-sub">' + summary(o) + '</span></span></button>' +
			'<span class="npc-badges">' + eventBadges(state.npcEvents[o.name.trim()], { replace: 'replaced by', remove: 'taken out by' }) +
			(o.random ? '<span class="badge text-bg-info">one random location</span>' : '') +
			(error ? '<span class="badge text-bg-danger" title="' + esc(error) + '">needs fixing</span>' : '') + '</span>';
	}

	function equipmentHtml(o) {
		return '<div class="npc-chips">' + (o.equipment.length ? o.equipment.map(function (lot, i) {
			var name = itemName(lot);
			return '<span class="badge npc-chip">' + esc(lot) + (name ? ' · ' + esc(name) : '') +
				'<button type="button" class="btn-close" data-action="remove-equip" data-index="' + i + '" aria-label="Remove ' + esc(name || lot) + '"></button></span>';
		}).join('') : '<span class="small text-body-secondary">Nothing equipped</span>') + '</div>' +
			'<div class="input-group input-group-sm npc-equip-add"><input type="text" inputmode="numeric" class="form-control" data-equip-input placeholder="Item LOT, e.g. 6802" aria-label="Item LOT to equip">' +
			'<button type="button" class="btn btn-outline-secondary" data-action="add-equip">Add</button></div>';
	}

	function bodyHtml(o) {
		var id = 'npc' + o.id;
		var phraseCount = lines(o.phrases).length;
		return '<div class="card-body">' +
			'<div class="npc-row">' +
			'<div class="npc-name"><label class="form-label small" for="' + id + '-name">Name</label>' +
			'<input class="form-control form-control-sm' + (fieldError('name', o.name) ? ' is-invalid' : '') + '" id="' + id + '-name" data-field="name" maxlength="100" value="' + esc(o.name) + '"></div>' +
			'<div class="npc-lot"><label class="form-label small" for="' + id + '-lot">LOT</label>' +
			'<div class="d-flex align-items-center gap-2"><input type="number" min="1" step="1" class="form-control form-control-sm' + (fieldError('lot', o.lot) ? ' is-invalid' : '') + '" id="' + id + '-lot" data-field="lot" value="' + esc(o.lot) + '">' +
			'<span class="small text-body-secondary text-truncate" data-lot-name>' + esc(itemName(o.lot) || (o.lot ? 'no item name' : '')) + '</span></div></div>' +
			'</div>' +
			'<div class="npc-section"><div class="form-label small">Equipment</div>' + equipmentHtml(o) + '</div>' +
			'<div class="npc-section"><label class="form-label small" for="' + id + '-phrases">Phrases <span class="text-body-secondary">(one per line, said at random)</span></label>' +
			'<textarea class="form-control form-control-sm" id="' + id + '-phrases" data-field="phrases" rows="' + Math.min(10, Math.max(3, phraseCount + 1)) + '">' + esc(o.phrases) + '</textarea></div>' +
			'<div class="npc-section"><label class="form-label small" for="' + id + '-config">Config <span class="text-body-secondary">(one per line)</span></label>' +
			'<textarea class="form-control form-control-sm font-monospace' + (configError(o.config) ? ' is-invalid' : '') + '" id="' + id + '-config" data-field="config" spellcheck="false" rows="' +
			Math.min(8, Math.max(2, lines(o.config).length + 1)) + '">' + esc(o.config) + '</textarea>' +
			'<div class="invalid-feedback" data-config-error>' + esc(configError(o.config)) + '</div>' +
			'<div class="form-text">Written <code>name=type:value</code>, e.g. <code>custom_script_client=0:scripts\\ai\\SPEC\\MISSION_MINIGAME_CLIENT.lua</code>. Types: 0 text, 1 whole number, 3 decimal, 5 unsigned, 7 true/false (1 or 0), 13 UTF-8 text.</div></div>' +
			'<div class="npc-section"><div class="d-flex flex-wrap justify-content-between align-items-center gap-2 mb-1">' +
			'<div class="form-label small mb-0">Locations</div>' +
			'<div class="d-flex flex-wrap gap-3">' +
			'<div class="form-check mb-0"><input class="form-check-input" type="checkbox" id="' + id + '-random" data-random' + (o.random ? ' checked' : '') + '>' +
			'<label class="form-check-label small" for="' + id + '-random">Appear at one random location</label></div>' +
			'<div class="form-check form-switch mb-0"><input class="form-check-input" type="checkbox" role="switch" id="' + id + '-raw" data-action="raw"' + (state.raw[o.id] ? ' checked' : '') + '>' +
			'<label class="form-check-label small" for="' + id + '-raw">Advanced rotation</label></div></div></div>' +
			(o.random ? '<p class="small text-body-secondary mb-1">Each time the world starts, the NPC appears at one of these, picked at random, instead of at all of them.</p>' : '') +
			'<div class="npc-locs">' + o.locations.map(function (l, i) { return locationHtml(o, l, i); }).join('') + '</div>' +
			'<button type="button" class="btn btn-outline-secondary btn-sm mt-1" data-action="add-loc">Add location</button>' +
			'<div class="form-text">Use <code>/pos</code> and <code>/rot</code> in game to read where you stand. Facing 0 looks along +Z.</div></div>' +
			'</div>' +
			'<div class="card-footer d-flex flex-wrap gap-2 justify-content-end">' +
			'<button type="button" class="btn btn-outline-secondary btn-sm" data-action="duplicate">Duplicate</button>' +
			'<button type="button" class="btn btn-outline-danger btn-sm" data-action="delete">Delete NPC</button></div>';
	}

	function cardHtml(o) {
		return '<section class="card npc-card' + (state.open[o.id] ? ' is-open' : '') + (matches(o) ? '' : ' d-none') + '" data-npc="' + o.id + '">' +
			'<div class="card-header npc-head">' + headHtml(o) + '</div>' + (state.open[o.id] ? bodyHtml(o) : '') + '</section>';
	}

	function matches(o) {
		if (!state.filter) return true;
		var text = [o.name, o.lot, itemName(o.lot), o.phrases, o.config].concat(o.locations.map(function (l) { return zoneLabel(l.zone); })).join(' ').toLowerCase();
		return text.indexOf(state.filter) !== -1;
	}

	function statusLine(name) {
		var f = fileInfo(name);
		if (!f) return '';
		if (f.root) return 'The file the worlds start from. It can hold NPCs too.';
		// An event that is on now loads it differently from what the files say
		if (f.exists && f.loadedNow !== f.loaded) {
			return (f.loadedNow ? 'Loaded in game now: switched on by ' : 'Not loaded in game now: switched off by ') + (f.switchedNow || 'an event') +
				', which is on (' + (f.loaded ? 'loaded' : 'not loaded') + ' without events)';
		}
		return f.loaded ? 'Loaded in game' : 'Not loaded in game: ' + notLoadedWhy(f).replace(/^./, function (c) { return c.toLowerCase(); });
	}

	function includesHtml() {
		var name = state.selected.name;
		var rows = state.includes.map(function (inc, i) {
			var f = fileInfo(inc.name), id = 'inc-' + i;
			return '<div class="vanity-include" data-inc="' + i + '">' +
				'<div class="form-check form-switch mb-0"><input class="form-check-input" type="checkbox" role="switch" id="' + id + '" data-inc-toggle' + (inc.enabled ? ' checked' : '') + '>' +
				'<label class="form-check-label" for="' + id + '">' + esc(inc.name) + '</label></div>' +
				(f && f.exists ? '<span class="small text-body-secondary">' + (f.error ? '' : plural(f.npcs, 'NPC')) + '</span>' : '<span class="badge text-bg-warning" title="Not in the folder">missing</span>') +
				'<button type="button" class="btn btn-link btn-sm link-danger ms-auto p-0" data-action="remove-include" aria-label="Stop including ' + esc(inc.name) + '">Remove</button></div>';
		}).join('');
		var options = state.files.filter(function (f) {
			return !f.root && f.name !== name && f.exists && !state.includes.some(function (inc) { return inc.name === f.name; });
		}).map(function (f) { return '<option value="' + esc(f.name) + '">'; }).join('');
		return '<section class="card mb-3 vanity-includes"><div class="card-header py-2"><h4 class="h6 mb-0">Includes</h4>' +
			'<div class="small text-body-secondary">Files this one loads when it is loaded itself. Saved with the file.</div></div>' +
			'<div class="card-body py-2">' + (rows || '<p class="small text-body-secondary mb-2">This file includes no other files.</p>') +
			'<div class="input-group input-group-sm vanity-include-add"><input type="text" class="form-control" list="vanityIncludeOptions" data-include-input placeholder="File name, e.g. my-npcs.xml" aria-label="File to include" spellcheck="false">' +
			'<button type="button" class="btn btn-outline-secondary" data-action="add-include">Add include</button></div>' +
			'<datalist id="vanityIncludeOptions">' + options + '</datalist></div></section>';
	}

	function fileHtml() {
		var name = state.selected.name, info = fileInfo(name);
		var head = '<div class="vanity-main-head"><div><h3 class="h5 mb-0">' + esc(name) + ' <span class="vanity-head-badge">' + (info ? fileBadges(info) : '') + '</span></h3>' +
			'<div class="small text-body-secondary" data-status>' + esc(statusLine(name)) + '</div></div>' +
			(info && info.root ? '' : '<button type="button" class="btn btn-outline-danger btn-sm" data-action="delete-file">Delete file</button>') + '</div>';
		if (state.loadError) {
			return head + '<div class="alert alert-danger">' + esc(state.loadError) + '<div class="small mt-1">Fix it by hand in <code>' + esc(el.folder.textContent + '/' + name) + '</code>, then reload this page.</div></div>';
		}
		if (!state.objects) return head + '<p class="text-body-secondary">Loading&hellip;</p>';
		var list = state.objects.length ? state.objects.map(cardHtml).join('') : '<p class="text-body-secondary">No NPCs in this file' + (info && info.root ? '; root.xml usually only lists the other files.' : ' yet.') + '</p>';
		return head + includesHtml() +
			'<div class="vanity-toolbar d-flex flex-wrap align-items-center gap-2 mb-2">' +
			'<input type="search" class="form-control form-control-sm" id="npcFilter" placeholder="Filter by name, LOT, item, zone or phrase" aria-label="Filter NPCs" value="' + esc(state.filter) + '">' +
			'<button type="button" class="btn btn-outline-secondary btn-sm" data-action="expand">' + (allOpen() ? 'Collapse all' : 'Expand all') + '</button>' +
			'<button type="button" class="btn btn-primary btn-sm ms-auto" data-action="add-npc">Add NPC</button></div>' +
			'<div class="small text-body-secondary mb-2">' + plural(state.objects.length, 'NPC') + '. Saving rewrites the file; comments in it are not kept (the old file stays as <code>' + esc(name) + '.bak</code>).</div>' +
			'<div id="npcList">' + list + '</div>';
	}

	function renderIncludes() {
		var section = el.main.querySelector('.vanity-includes');
		if (section) section.outerHTML = includesHtml();
		renderSaveBar();
	}

	function allOpen() { return state.objects && state.objects.length && state.objects.every(function (o) { return state.open[o.id]; }); }

	function textHtml() {
		var t = state.text;
		if (state.loadError) return '<div class="alert alert-danger">' + esc(state.selected.name + ': ' + state.loadError) + '</div>';
		if (!t) return '<p class="text-body-secondary">Loading&hellip;</p>';
		return '<div class="vanity-main-head"><div><h3 class="h5 mb-0">' + esc(t.name) + '</h3><div class="small text-body-secondary">' + esc(t.where) + '</div></div></div>' +
			'<div class="card"><div class="card-body">' +
			'<label class="form-label small" for="vanityText">Text (Markdown)</label>' +
			'<textarea class="form-control font-monospace vanity-text" id="vanityText" data-text rows="18" spellcheck="true">' + esc(t.value) + '</textarea>' +
			'<div class="d-flex justify-content-end mt-2"><button type="button" class="btn btn-primary btn-sm" data-action="save-text"' + (isDirty() ? '' : ' disabled') + '>Save</button></div>' +
			'</div></div>';
	}

	function renderMain() {
		if (!state.selected) el.main.innerHTML = '<p class="text-body-secondary">Pick a file or a text on the left.</p>';
		else el.main.innerHTML = state.selected.kind === 'file' ? fileHtml() : textHtml();
		renderSaveBar();
	}

	function renderCard(o, focusSelector) {
		var card = el.main.querySelector('[data-npc="' + o.id + '"]');
		if (!card) return;
		card.outerHTML = cardHtml(o);
		if (focusSelector) {
			var target = el.main.querySelector('[data-npc="' + o.id + '"] ' + focusSelector);
			if (target) target.focus();
		}
	}

	function refreshHead(o) {
		var head = el.main.querySelector('[data-npc="' + o.id + '"] .npc-head');
		if (head) head.innerHTML = headHtml(o);
	}

	function renderSaveBar() {
		var dirty = isDirty();
		el.saveBar.classList.toggle('d-none', !dirty);
		if (dirty) el.saveLabel.textContent = 'Unsaved changes to ' + state.selected.name;
		el.save.textContent = state.selected && state.selected.kind === 'text' ? 'Save text' : 'Save file';
		el.save.disabled = state.saving;
		var textSave = el.main.querySelector('[data-action="save-text"]');
		if (textSave) textSave.disabled = state.saving || !dirty;
	}

	function fileInfo(name) { return state.files.filter(function (f) { return f.name === name; })[0]; }

	// ---------------------------------------------------------------- loading

	function loadList() {
		return api.get('/api/vanity').then(function (d) {
			if (!d.success) { toast(d.error || 'Could not load the vanity files', 'danger'); return; }
			state.files = d.files;
			state.texts = d.texts;
			state.events = d.events || [];
			state.zones = d.zones;
			state.zoneNames = {};
			d.zones.forEach(function (z) { state.zoneNames[z.id] = z.name; });
			el.folder.textContent = d.folder;
			el.disabled.classList.toggle('d-none', !d.disabled);
			el.rootError.classList.toggle('d-none', !d.rootError);
			el.rootError.textContent = d.rootError ? 'root.xml: ' + d.rootError : '';
			renderSide();
			// For the Preview tab
			document.dispatchEvent(new CustomEvent('vanity:data', { detail: d }));
		});
	}

	function select(kind, name, force) {
		if (!force && isDirty() && !confirm('Discard your unsaved changes to ' + state.selected.name + '?')) return false;
		state.selected = name ? { kind: kind, name: name } : null;
		state.objects = null;
		state.includes = null;
		state.text = null;
		state.loadError = '';
		state.filter = '';
		state.open = {};
		state.raw = {};
		if (filesTab()) setHash(name ? '#' + kind + ':' + name : '');
		renderSide();
		renderMain();
		if (!name) return true;
		if (kind === 'file') {
			api.get('/api/vanity/files/' + encodeURIComponent(name)).then(function (d) {
				if (!state.selected || state.selected.name !== name) return;
				if (!d.success) { state.loadError = d.error || 'Could not read ' + name; renderMain(); return; }
				Object.keys(d.itemNames || {}).forEach(function (lot) { state.itemNames[lot] = d.itemNames[lot]; });
				state.npcEvents = d.npcEvents || {};
				state.objects = d.objects.map(fromServer);
				state.baseline = serialize();
				state.includes = (d.files || []).map(function (f) { return { name: f.name, enabled: !!f.enabled }; });
				state.includesBaseline = JSON.stringify(state.includes);
				if (state.objects.length === 1) state.open[state.objects[0].id] = true;
				renderMain();
			});
		} else {
			var info = state.texts.filter(function (t) { return t.name === name; })[0] || { where: '' };
			api.get('/api/vanity/text/' + encodeURIComponent(name)).then(function (d) {
				if (!state.selected || state.selected.name !== name) return;
				if (!d.success) { state.loadError = d.error || 'Could not read ' + name; renderMain(); return; }
				state.text = { name: name, where: info.where, value: d.text, baseline: d.text };
				renderMain();
			});
		}
		return true;
	}

	function filesTab() { return el.tabFiles.classList.contains('active'); }

	function setHash(hash) {
		try { history.replaceState(null, '', hash || location.pathname); } catch (err) {}
	}

	function selectDefault() {
		var hash = decodeURIComponent(location.hash.slice(1)), colon = hash.indexOf(':');
		if (hash.indexOf('preview') === 0) bootstrap.Tab.getOrCreateInstance(document.getElementById('vanityTabPreview')).show();
		var kind = hash.slice(0, colon), name = hash.slice(colon + 1);
		if (kind === 'file' && fileInfo(name)) return select('file', name, true);
		if (kind === 'text' && state.texts.some(function (t) { return t.name === name; })) return select('text', name, true);
		var first = state.files.filter(function (f) { return !f.root && f.loaded && f.exists; })[0] || fileInfo(ROOT) || state.files[0];
		select(first ? 'file' : 'text', first ? first.name : (state.texts[0] || {}).name, true);
	}

	// ---------------------------------------------------------------- saving

	function save() {
		if (!isDirty() || state.saving) return;
		if (state.selected.kind === 'text') return saveText();
		for (var i = 0; i < state.objects.length; i++) {
			var error = npcError(state.objects[i]);
			if (error) {
				var o = state.objects[i];
				state.open[o.id] = true;
				state.filter = '';
				renderMain();
				var card = el.main.querySelector('[data-npc="' + o.id + '"]');
				if (card) card.scrollIntoView({ block: 'start', behavior: 'smooth' });
				toast('Fix this first: ' + error, 'danger');
				return;
			}
		}
		var name = state.selected.name, sent = serialize(), sentIncludes = JSON.stringify(state.includes);
		var body = { objects: JSON.parse(sent) };
		if (includesDirty()) body.files = JSON.parse(sentIncludes);
		state.saving = true;
		renderSaveBar();
		api.post('/api/vanity/files/' + encodeURIComponent(name), body).then(function (d) {
			state.saving = false;
			if (!d.success) { toast(d.error || 'Could not save', 'danger'); renderSaveBar(); return; }
			toast(d.message, 'success');
			if (state.selected && state.selected.name === name) { state.baseline = sent; state.includesBaseline = sentIncludes; }
			renderSaveBar();
			loadList().then(refreshFileHead);
		}).catch(function () { state.saving = false; renderSaveBar(); });
	}

	function saveText() {
		var t = state.text, value = t.value;
		state.saving = true;
		renderSaveBar();
		api.post('/api/vanity/text/' + encodeURIComponent(t.name), { text: value }).then(function (d) {
			state.saving = false;
			if (!d.success) { toast(d.error || 'Could not save', 'danger'); renderSaveBar(); return; }
			toast(d.message, 'success');
			t.baseline = value;
			renderSaveBar();
		}).catch(function () { state.saving = false; renderSaveBar(); });
	}

	function discard() {
		if (!state.selected) return;
		if (state.selected.kind === 'text') { state.text.value = state.text.baseline; renderMain(); return; }
		select('file', state.selected.name, true);
	}

	// ---------------------------------------------------------------- editing

	function onCardInput(target, finished) {
		var card = target.closest('[data-npc]');
		var o = card && byId(card.dataset.npc);
		if (!o) return;

		if (target.matches('[data-field]')) {
			var field = target.dataset.field;
			o[field] = target.value;
			if (field === 'name' || field === 'lot') target.classList.toggle('is-invalid', !!fieldError(field, target.value));
			if (field === 'config') {
				var error = configError(target.value);
				target.classList.toggle('is-invalid', !!error);
				card.querySelector('[data-config-error]').textContent = error;
			}
			if (field === 'lot' && finished) {
				fetchNames([o.lot]).then(function () {
					var span = card.querySelector('[data-lot-name]');
					if (span) span.textContent = itemName(o.lot) || (o.lot ? 'no item name' : '');
					refreshHead(o);
				});
			}
			refreshHead(o);
		} else if (target.matches('[data-random]')) {
			o.random = target.checked;
			renderCard(o, '[data-random]');
		} else if (target.matches('[data-loc-field]')) {
			var row = target.closest('[data-loc]');
			var l = o.locations[Number(row.dataset.loc)];
			var f = target.dataset.locField;
			target.classList.toggle('is-invalid', !!fieldError(f, target.value));
			if (f === 'facing') {
				if (target.value.trim() !== '' && isFinite(Number(target.value))) {
					setFacing(l, target.value);
					['rw', 'rx', 'ry', 'rz'].forEach(function (k) {
						var input = row.querySelector('[data-loc-field="' + k + '"]');
						if (input) { input.value = l[k]; input.classList.remove('is-invalid'); }
					});
				}
			} else {
				l[f] = target.value;
				if (f === 'rw' || f === 'rx' || f === 'ry' || f === 'rz') {
					var facingInput = row.querySelector('[data-loc-field="facing"]');
					facingInput.value = facing(l);
					facingInput.placeholder = yawOnly(l) ? '' : 'tilted';
				}
			}
			refreshHead(o);
		} else if (target.matches('[data-equip-input]')) {
			return;
		}
		renderSaveBar();
	}

	function addEquipment(o, card) {
		var input = card.querySelector('[data-equip-input]');
		var lots = input.value.split(/[\s,]+/).filter(Boolean);
		var bad = lots.filter(function (v) { return !/^\d+$/.test(v) || Number(v) <= 0; });
		if (!lots.length) { input.focus(); return; }
		if (bad.length) { toast('"' + bad[0] + '" is not an item LOT', 'warning'); input.focus(); return; }
		lots.forEach(function (v) { o.equipment.push(Number(v)); });
		fetchNames(lots).then(function () { renderCard(o, '[data-equip-input]'); renderSaveBar(); });
	}

	function onCardAction(button) {
		var action = button.dataset.action;
		var card = button.closest('[data-npc]');
		var o = card && byId(card.dataset.npc);
		var index;

		switch (action) {
		case 'toggle':
			state.open[o.id] = !state.open[o.id];
			renderCard(o, '[data-action="toggle"]');
			break;
		case 'raw':
			state.raw[o.id] = button.checked;
			renderCard(o, '[data-action="raw"]');
			break;
		case 'add-loc':
			o.locations.push(newLocation(o.locations[o.locations.length - 1]));
			renderCard(o, '[data-loc="' + (o.locations.length - 1) + '"] [data-loc-field="x"]');
			break;
		case 'remove-loc':
			index = Number(button.closest('[data-loc]').dataset.loc);
			o.locations.splice(index, 1);
			renderCard(o);
			break;
		case 'add-equip':
			addEquipment(o, card);
			break;
		case 'remove-equip':
			o.equipment.splice(Number(button.dataset.index), 1);
			renderCard(o, '[data-equip-input]');
			break;
		case 'duplicate':
			var copy = JSON.parse(JSON.stringify(o));
			copy.id = state.nextId++;
			copy.name = o.name + ' (copy)';
			state.objects.splice(state.objects.indexOf(o) + 1, 0, copy);
			state.open[copy.id] = true;
			renderMain();
			focusCard(copy);
			break;
		case 'delete':
			if (!confirm('Delete "' + (o.name || 'this NPC') + '"? It goes when you save the file.')) return;
			state.objects.splice(state.objects.indexOf(o), 1);
			renderMain();
			break;
		case 'add-npc':
			var npc = fromServer({ name: 'New NPC', lot: DEFAULT_LOT, locations: [{ zone: DEFAULT_ZONE, x: 0, y: 0, z: 0, rw: 1, rx: 0, ry: 0, rz: 0 }] });
			state.objects.push(npc);
			state.open[npc.id] = true;
			state.filter = '';
			fetchNames([DEFAULT_LOT]).then(function () { renderMain(); focusCard(npc); });
			break;
		case 'expand':
			var open = !allOpen();
			state.objects.forEach(function (x) { state.open[x.id] = open; });
			renderMain();
			break;
		case 'add-include':
			addInclude();
			break;
		case 'remove-include':
			state.includes.splice(Number(button.closest('[data-inc]').dataset.inc), 1);
			renderIncludes();
			break;
		case 'delete-file':
			deleteFile();
			break;
		case 'save-text':
			saveText();
			break;
		}
		renderSaveBar();
	}

	function focusCard(o) {
		var card = el.main.querySelector('[data-npc="' + o.id + '"]');
		if (!card) return;
		card.scrollIntoView({ block: 'start', behavior: 'smooth' });
		var name = card.querySelector('[data-field="name"]');
		if (name) { name.focus({ preventScroll: true }); name.select(); }
	}

	// ---------------------------------------------------------------- files

	// A file on disk changed its includes (switch, new file): keep the open editor in step, without touching other edits
	function applyInclude(parent, name, enabled) {
		if (!state.selected || state.selected.kind !== 'file' || state.selected.name !== parent || !state.includes) return;
		var apply = function (list) {
			var entry = list.filter(function (inc) { return inc.name === name; })[0];
			if (entry) entry.enabled = enabled; else list.push({ name: name, enabled: enabled });
			return list;
		};
		apply(state.includes);
		state.includesBaseline = JSON.stringify(apply(JSON.parse(state.includesBaseline || '[]')));
		renderIncludes();
	}

	// Switch one include (parent -> name) on or off, straight away
	function setInclude(name, parent, enabled, control) {
		if (state.selected && state.selected.name === parent && includesDirty()) {
			toast('Save or discard your changes to ' + parent + '\'s includes first', 'warning');
			if (control) control.checked = !enabled;
			return;
		}
		if (control) control.disabled = true;
		api.action('/api/vanity/root', { name: name, enabled: enabled, parent: parent }).then(function (d) {
			toast(d.message, 'success');
			applyInclude(parent, name, enabled);
		}).catch(function () {}).then(function () { loadList().then(refreshFileHead); });
	}

	// The status under the file name; the NPCs being edited stay as they are
	function refreshFileHead() {
		if (!state.selected || state.selected.kind !== 'file') return;
		var info = fileInfo(state.selected.name);
		var line = el.main.querySelector('.vanity-main-head [data-status]');
		if (line) line.textContent = statusLine(state.selected.name);
		var badge = el.main.querySelector('.vanity-head-badge');
		if (badge) badge.innerHTML = info ? fileBadges(info) : '';
		if (state.includes) renderIncludes();
	}

	function newFile() {
		var parent = document.getElementById('newFileParent');
		parent.innerHTML = state.files.filter(function (f) { return f.exists && !f.error; }).map(function (f) {
			return '<option value="' + esc(f.name) + '"' + (f.root ? ' selected' : '') + '>' + esc(f.name) + '</option>';
		}).join('');
		document.getElementById('newFileName').value = '';
		newFileModal.show();
	}

	function createFile(e) {
		e.preventDefault();
		var name = document.getElementById('newFileName').value.trim(), parent = document.getElementById('newFileParent').value;
		if (state.selected && state.selected.name === parent && includesDirty()) { toast('Save or discard your changes to ' + parent + '\'s includes first', 'warning'); return; }
		if (isDirty() && !confirm('Discard your unsaved changes to ' + state.selected.name + '?')) return;
		api.action('/api/vanity/files', { name: name, parent: parent }).then(function (d) {
			toast(d.message, 'success');
			newFileModal.hide();
			return loadList().then(function () { select('file', d.name, true); });
		}).catch(function () {});
	}

	function addInclude() {
		var input = el.main.querySelector('[data-include-input]');
		var name = input.value.trim();
		if (name && !/\.xml$/i.test(name)) name += '.xml';
		if (!FILE_NAME.test(name)) { toast('Use letters, digits, - and _ for the file name', 'warning'); input.focus(); return; }
		if (name === ROOT || name === state.selected.name) { toast(name + ' can\'t be included here', 'warning'); input.focus(); return; }
		if (state.includes.some(function (inc) { return inc.name === name; })) { toast(name + ' is already included', 'info'); return; }
		state.includes.push({ name: name, enabled: true });
		renderIncludes();
		var again = el.main.querySelector('[data-include-input]');
		if (again) again.focus();
	}

	function deleteFile() {
		var name = state.selected.name;
		var by = (fileInfo(name) || {}).includedBy || [];
		if (!confirm('Delete ' + name + '?' + (by.length ? ' It is taken out of ' + by.map(function (b) { return b.file; }).join(', ') + '.' : '') + ' A copy is kept as ' + name + '.bak.')) return;
		api.action('/api/vanity/files/' + encodeURIComponent(name) + '/delete', {}).then(function (d) {
			toast(d.message, 'success');
			state.selected = null;
			return loadList().then(function () { setHash(''); selectDefault(); });
		}).catch(function () {});
	}

	function respawn() {
		el.reload.disabled = true;
		api.action('/api/vanity/reload', {}, null).then(function (d) {
			if (d.result && d.result.then) return d.result;
			toast('Respawn requested', 'info');
		}).catch(function () {}).then(function () { el.reload.disabled = false; });
	}

	// ---------------------------------------------------------------- events

	el.files.addEventListener('click', function (e) {
		var link = e.target.closest('[data-select-file]');
		if (!link) return;
		e.preventDefault();
		if (!state.selected || state.selected.kind !== 'file' || state.selected.name !== link.dataset.selectFile) select('file', link.dataset.selectFile);
	});
	el.files.addEventListener('change', function (e) {
		if (e.target.matches('[data-enable]')) setInclude(e.target.dataset.enable, e.target.dataset.parent, e.target.checked, e.target);
	});
	el.files.addEventListener('click', function (e) {
		var include = e.target.closest('[data-include-root]');
		if (include) setInclude(include.dataset.includeRoot, ROOT, false, null);
	});
	el.texts.addEventListener('click', function (e) {
		var link = e.target.closest('[data-select-text]');
		if (!link) return;
		e.preventDefault();
		if (!state.selected || state.selected.kind !== 'text' || state.selected.name !== link.dataset.selectText) select('text', link.dataset.selectText);
	});

	el.main.addEventListener('input', function (e) {
		if (e.target.id === 'npcFilter') {
			state.filter = e.target.value.trim().toLowerCase();
			(state.objects || []).forEach(function (o) {
				var card = el.main.querySelector('[data-npc="' + o.id + '"]');
				if (card) card.classList.toggle('d-none', !matches(o));
			});
			return;
		}
		if (e.target.matches('[data-text]')) { state.text.value = e.target.value; renderSaveBar(); return; }
		if (!e.target.matches('[data-random], [data-action="raw"]')) onCardInput(e.target, false);
	});
	el.main.addEventListener('change', function (e) {
		if (e.target.matches('[data-inc-toggle]')) {
			state.includes[Number(e.target.closest('[data-inc]').dataset.inc)].enabled = e.target.checked;
			renderSaveBar();
			return;
		}
		if (e.target.matches('[data-action="raw"]')) { onCardAction(e.target); return; }
		if (e.target.matches('select, [data-random], [data-field="lot"]')) onCardInput(e.target, true);
	});
	el.main.addEventListener('click', function (e) {
		var button = e.target.closest('button[data-action]');
		if (button) onCardAction(button);
	});
	el.main.addEventListener('keydown', function (e) {
		if (e.key === 'Enter' && e.target.matches('[data-include-input]')) { e.preventDefault(); addInclude(); return; }
		if (e.key === 'Enter' && e.target.matches('[data-equip-input]')) {
			e.preventDefault();
			addEquipment(byId(e.target.closest('[data-npc]').dataset.npc), e.target.closest('[data-npc]'));
		}
	});

	// The tabs keep their place in the hash
	el.tabFiles.addEventListener('shown.bs.tab', function () { setHash(state.selected ? '#' + state.selected.kind + ':' + state.selected.name : ''); });
	document.getElementById('vanityTabPreview').addEventListener('shown.bs.tab', function () { if (location.hash.indexOf('#preview') !== 0) setHash('#preview'); });
	if (window.Live) Live.on('scheduled_events', Live.throttle(function () { loadList().then(refreshFileHead); }, 1000));

	var newFileModal = new bootstrap.Modal(document.getElementById('newFileModal'));
	el.newFile.addEventListener('click', newFile);
	document.getElementById('newFileForm').addEventListener('submit', createFile);
	el.reload.addEventListener('click', respawn);
	el.save.addEventListener('click', save);
	el.discard.addEventListener('click', function () { if (confirm('Throw away your unsaved changes?')) discard(); });
	document.addEventListener('keydown', function (e) {
		if ((e.ctrlKey || e.metaKey) && e.key === 's' && isDirty() && filesTab()) { e.preventDefault(); save(); }
	});
	window.addEventListener('beforeunload', function (e) {
		if (isDirty()) { e.preventDefault(); e.returnValue = ''; }
	});

	loadList().then(selectDefault);
})();
