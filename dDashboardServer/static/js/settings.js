/**
 * The Settings page.
 *
 * - Model: what /api/settings returns (categories > sections > settings) plus the edits not saved yet (`pending`).
 *   Nothing is sent until Save; then every edit goes in one request, and the server saves all of them or none.
 * - Controls: one entry per setting type, each able to render its input, read it back and check a value.
 * - Conditions: a setting or section can depend on another setting ("only used when database_type is mysql"). They
 *   are checked against the value on screen, so switching hardcore mode on shows its settings straight away.
 * - Settings that only the .ini files or the environment can set (the database connection, the dashboard's keys) are
 *   shown read-only, with where to change them.
 */
(function () {
	'use strict';

	var el = {
		page: document.getElementById('settingsPage'),
		nav: document.getElementById('settingsNav'),
		body: document.getElementById('settingsBody'),
		search: document.getElementById('settingsSearch'),
		filter: document.getElementById('settingsFilter'),
		showUnused: document.getElementById('showUnused'),
		saveBar: document.getElementById('settingsSaveBar'),
		saveCount: document.getElementById('settingsSaveCount'),
		save: document.getElementById('settingsSave'),
		discard: document.getElementById('settingsDiscard'),
		restartNotice: document.getElementById('restartNotice'),
		restartList: document.getElementById('restartList')
	};

	var state = {
		categories: [], sections: [], settings: [], files: [], names: {},
		byKey: {},        // "file/name" -> setting
		controllers: {},  // keys other settings depend on
		pending: {},      // "file/name" -> { value: string | null (clear the value set here), webWins: bool }
		errors: {},       // "file/name" -> message
		category: null,
		query: '',
		filter: 'all',    // all | changed | restart
		saving: false,
		options: {},      // "zone" | "reward_code" -> [{id, label, detail}] from the client database
		openPicker: null  // { key, filter } kept open across redraws
	};

	// Lists whose values can be picked from the client database instead of typed
	var PICKABLE = { zone: 'Zones', reward_code: 'Reward codes' };


	// ---------------------------------------------------------------- model helpers

	function keyOf(s) { return s.file + '/' + s.name; }
	function domId(key) { return 'setting-' + key.replace(/[^a-z0-9_]/gi, '-'); }

	// The value shown now: an unsaved edit, else what the servers use
	function currentValue(s) {
		var p = state.pending[keyOf(s)];
		if (p) return p.value === null ? fallbackValue(s) : p.value;
		return s.value === undefined ? '' : String(s.value);
	}

	// What applies once the value set here is removed: the file's, else the default
	function fallbackValue(s) {
		if (s.fileSource && s.fileValue !== null && s.fileValue !== undefined && s.fileValue !== '') return String(s.fileValue);
		return s['default'] || '';
	}

	// What the page shows for a choice's value, e.g. "Shared loot (like live)" for 0
	function choiceLabel(s, value) {
		var i = s.choices ? s.choices.indexOf(value) : -1;
		return i >= 0 && s.choiceLabels ? s.choiceLabels[i] : value;
	}

	function isChanged(s) { return s.source !== 'default' && (s.secret || String(s.value) !== String(s['default'])); }
	function isPending(s) { return Object.prototype.hasOwnProperty.call(state.pending, keyOf(s)); }

	function conditionMet(condition) {
		if (!condition) return true;
		var controller = state.byKey[condition.file + '/' + condition.key];
		if (!controller) return true;
		var value = currentValue(controller) || controller['default'];
		return condition.values.indexOf(value) !== -1;
	}

	function describeCondition(condition) {
		var controller = state.byKey[condition.file + '/' + condition.key];
		if (!controller) return condition.key + ' is ' + condition.values.join(' or ');
		var shown = condition.values.map(function (v) { return controller.type === 'bool' ? (v === '1' ? 'on' : 'off') : choiceLabel(controller, v); });
		return controller.type === 'bool' && condition.values.length === 1
			? '<a href="#" data-goto="' + esc(keyOf(controller)) + '">' + esc(controller.title) + '</a> is ' + shown[0]
			: '<a href="#" data-goto="' + esc(keyOf(controller)) + '">' + esc(controller.title) + '</a> is ' + esc(shown.join(' or '));
	}

	function matchesFilters(s) {
		if (s.unused && !el.showUnused.checked && !isChanged(s) && !isPending(s)) return false;
		if (state.filter === 'changed' && !isChanged(s) && !isPending(s)) return false;
		if (state.filter === 'restart' && !s.restart) return false;
		if (!state.query) return true;
		return queryMatches(s.name + ' ' + s.title + ' ' + s.description + ' ' + s.section + ' ' + s.file);
	}

	// ---------------------------------------------------------------- fuzzy search
	// "rent, ugc icon": commas (or |) separate searches, any of which may match; within one search every word must
	// match somewhere, in any order. A word matches a word of the setting when it is part of it, when its letters
	// appear in it in order (e.g. "dbnc" -> "debounce"), or with one typo (words of 4+ letters).

	function searchGroups(query) {
		return query.split(/[,|]/).map(function (group) {
			return group.split(/[\s_.\-/]+/).filter(Boolean);
		}).filter(function (words) { return words.length; });
	}

	function isSubsequence(needle, word) {
		var i = 0;
		for (var j = 0; j < word.length && i < needle.length; j++) if (word[j] === needle[i]) i++;
		return i === needle.length;
	}

	// At most one insertion, deletion or substitution between needle and the start of word (so "unsed" finds "unused").
	function withinOneEdit(needle, word) {
		var candidates = [word.slice(0, needle.length - 1), word.slice(0, needle.length), word.slice(0, needle.length + 1)];
		return candidates.some(function (c) {
			var a = needle, b = c, edits = 0, i = 0, j = 0;
			while (i < a.length && j < b.length) {
				if (a[i] === b[j]) { i++; j++; continue; }
				if (++edits > 1) return false;
				if (a[i] === b[j + 1] && a[i + 1] === b[j]) { i += 2; j += 2; } // two letters swapped
				else if (a.length > b.length) i++;
				else if (b.length > a.length) j++;
				else { i++; j++; }
			}
			return edits + (a.length - i) + (b.length - j) <= 1;
		});
	}

	function wordMatches(needle, text, words) {
		if (text.indexOf(needle) !== -1) return true;
		return words.some(function (w) {
			if (needle.length >= 3 && needle[0] === w[0] && isSubsequence(needle, w)) return true;
			return needle.length >= 4 && withinOneEdit(needle, w);
		});
	}

	function queryMatches(haystack) {
		var text = haystack.toLowerCase().replace(/[_.\-/]+/g, ' ');
		var words = text.split(/\s+/).filter(Boolean);
		return searchGroups(state.query).some(function (group) {
			return group.every(function (needle) { return wordMatches(needle, text, words); });
		});
	}

	// ---------------------------------------------------------------- controls, one per setting type

	function numberError(s, value) {
		var n = Number(value);
		if (!isFinite(n) || (s.type === 'int' && Math.floor(n) !== n)) return s.type === 'int' ? 'Enter a whole number' : 'Enter a number';
		if (s.min !== undefined && n < s.min) return 'At least ' + s.min;
		if (s.max !== undefined && n > s.max) return 'At most ' + s.max;
		return '';
	}

	function textError(s, value) {
		if (s.format === 'url' && !/^https?:\/\//.test(value)) return 'Start with http:// or https://';
		if (s.format === 'email' && (value.indexOf('@') === -1 || /[\s,;]/.test(value))) return 'Enter one email address';
		if (s.format === 'host' && /[\s\/]/.test(value)) return 'A host name or address, without spaces or slashes';
		if (!s.multiline && /[\r\n]/.test(value)) return 'Use one line';
		return '';
	}

	// Wraps an input with the setting's unit, e.g. [ 30 | days ]
	function withUnit(s, input) {
		return s.unit ? '<div class="input-group input-group-sm">' + input + '<span class="input-group-text">' + esc(s.unit) + '</span></div>' : input;
	}

	function attrs(s) {
		return ' id="' + domId(keyOf(s)) + '" data-input="' + esc(keyOf(s)) + '"';
	}

	var Controls = {
		bool: {
			render: function (s, value) {
				return '<div class="form-check form-switch mb-0"><input class="form-check-input" type="checkbox" role="switch"' + attrs(s) +
					(value === '1' ? ' checked' : '') + '><label class="form-check-label small" for="' + domId(keyOf(s)) + '">' + (value === '1' ? 'On' : 'Off') + '</label></div>';
			},
			read: function (input) { return input.checked ? '1' : '0'; },
			check: function () { return ''; }
		},
		choice: {
			render: function (s, value) {
				return '<select class="form-select form-select-sm"' + attrs(s) + '>' + s.choices.map(function (c) {
					return '<option value="' + esc(c) + '"' + (c === value ? ' selected' : '') + '>' + esc(choiceLabel(s, c)) + (c === s['default'] ? ' (default)' : '') + '</option>';
				}).join('') + '</select>';
			},
			read: function (input) { return input.value; },
			check: function () { return ''; }
		},
		int: {
			render: function (s, value) {
				return withUnit(s, '<input type="number" class="form-control form-control-sm"' + attrs(s) + ' value="' + esc(value) + '"' +
					(s.min !== undefined ? ' min="' + s.min + '"' : '') + (s.max !== undefined ? ' max="' + s.max + '"' : '') +
					' step="' + (s.type === 'float' ? 'any' : '1') + '" placeholder="' + esc(s['default']) + '">');
			},
			read: function (input) { return input.value.trim(); },
			check: numberError
		},
		text: {
			render: function (s, value) {
				if (s.multiline) return '<textarea class="form-control form-control-sm" rows="3"' + attrs(s) + '>' + esc(value) + '</textarea>';
				var type = s.format === 'url' ? 'url' : s.format === 'email' ? 'email' : 'text';
				var mono = s.format === 'path' || s.format === 'host' ? ' font-monospace' : '';
				// Suggested values for a free text setting (e.g. model IDs): offered, not enforced
				var list = s.choices && s.choices.length ? domId(keyOf(s)) + 'Choices' : '';
				return withUnit(s, '<input type="' + type + '" class="form-control form-control-sm' + mono + '"' + attrs(s) + ' value="' + esc(value) + '"' +
					' placeholder="' + esc(s['default'] || '') + '" spellcheck="false"' + (list ? ' list="' + esc(list) + '"' : '') + '>' +
					(list ? '<datalist id="' + esc(list) + '">' + s.choices.map(function (c) { return '<option value="' + esc(c) + '">'; }).join('') + '</datalist>' : ''));
			},
			read: function (input, s) { return s.multiline ? input.value : input.value.trim(); },
			check: textError
		},
		secret: {
			render: function (s) {
				var p = state.pending[keyOf(s)];
				return '<input type="password" class="form-control form-control-sm"' + attrs(s) + ' value="' + esc(p && p.value ? p.value : '') + '" autocomplete="new-password"' +
					' placeholder="' + (s.isSet ? 'Set. Type a new one to replace it' : 'Not set') + '">';
			},
			read: function (input) { return input.value; },
			check: function () { return ''; }
		},
		int_list: {
			render: function (s, value) {
				var placeholder = { zone: 'Zone IDs, e.g. 1000, 1100', lot: 'Item or enemy LOTs, e.g. 6086', reward_code: 'Reward code IDs, e.g. 4, 30' }[s.listOf] || 'Numbers separated by commas';
				return '<input class="form-control form-control-sm font-monospace"' + attrs(s) + ' value="' + esc(value.split(',').filter(Boolean).join(', ')) + '"' +
					' placeholder="' + esc(placeholder) + '" inputmode="numeric" spellcheck="false">' + listNames(s, value);
			},
			read: function (input) { return input.value.split(',').map(function (v) { return v.trim(); }).filter(Boolean).join(','); },
			check: function (s, value) { return value.split(',').some(function (v) { return !/^\d+$/.test(v); }) ? 'Use whole numbers separated by commas' : ''; }
		}
	};
	Controls['float'] = Controls.int;

	// Chips for what's chosen, and a "Choose..." panel listing everything from the client database
	Controls.pick_list = {
		render: function (s, value) {
			var key = keyOf(s);
			var ids = value.split(',').filter(Boolean);
			var chips = ids.map(function (id) {
				return '<span class="badge text-bg-secondary pick-chip">' + esc(pickLabel(s, id)) +
					'<button type="button" class="btn-close btn-close-white" data-pick-remove="' + esc(key) + '" data-id="' + esc(id) + '" aria-label="Remove ' + esc(id) + '"></button></span>';
			}).join('');
			return '<input type="hidden"' + attrs(s) + ' value="' + esc(ids.join(',')) + '">' +
				'<div class="pick-chips">' + (chips || '<span class="small text-body-secondary">None</span>') + '</div>' +
				'<details class="pick-panel" data-picker="' + esc(key) + '"><summary class="btn btn-sm btn-outline-secondary">Choose&hellip;</summary>' +
				'<div class="pick-body"><input type="search" class="form-control form-control-sm mb-1" placeholder="Filter by name or ID" data-pick-filter aria-label="Filter">' +
				'<div data-pick-list><p class="small text-body-secondary mb-0 p-2">Loading&hellip;</p></div></div></details>';
		},
		read: function (input) { return input.value; },
		check: function (s, value) { return Controls.int_list.check(s, value); }
	};

	function pickLabel(s, id) {
		var option = (state.options[s.listOf] || []).filter(function (o) { return String(o.id) === String(id); })[0];
		var name = option ? option.label : (state.names[keyOf(s)] || {})[id];
		return id + (name ? ' \u00b7 ' + name : '');
	}

	function loadOptions(of) {
		if (state.options[of]) return Promise.resolve(state.options[of]);
		return api.get('/api/settings/options?of=' + of).then(function (d) { return (state.options[of] = d.success ? d.options : []); });
	}

	function fillPicker(details) {
		var s = state.byKey[details.dataset.picker];
		var chosen = currentValue(s).split(',').filter(Boolean);
		var filter = details.querySelector('[data-pick-filter]').value.trim().toLowerCase();
		loadOptions(s.listOf).then(function (options) {
			var shown = options.filter(function (o) { return !filter || (o.id + ' ' + o.label + ' ' + o.detail).toLowerCase().indexOf(filter) !== -1; });
			details.querySelector('[data-pick-list]').innerHTML = shown.length ? shown.map(function (o) {
				return '<label class="pick-option"><input class="form-check-input" type="checkbox" data-pick-toggle="' + esc(keyOf(s)) + '" value="' + esc(o.id) + '"' +
					(chosen.indexOf(String(o.id)) !== -1 ? ' checked' : '') + '><span><strong>' + esc(o.id) + ' &middot; ' + esc(o.label) + '</strong>' +
					(o.detail ? '<span class="d-block small text-body-secondary">' + esc(o.detail) + '</span>' : '') + '</span></label>';
			}).join('') : '<p class="small text-body-secondary mb-0 p-2">Nothing matches.</p>';
		});
	}

	// Add or remove one value, keeping the picker open through the redraw
	function pick(key, id, add) {
		var s = state.byKey[key];
		var ids = currentValue(s).split(',').filter(Boolean).filter(function (v) { return v !== String(id); });
		if (add) ids.push(String(id));
		ids.sort(function (a, b) { return Number(a) - Number(b); });
		var details = el.body.querySelector('[data-picker="' + CSS.escape(key) + '"]');
		state.openPicker = details && details.open ? { key: key, filter: details.querySelector('[data-pick-filter]').value } : null;
		edit(key, ids.join(','));
		render();
	}

	function control(s) {
		if (s.type === 'int_list' && PICKABLE[s.listOf]) return Controls.pick_list;
		return Controls[s.type] || Controls.text;
	}

	function checkValue(s, value) {
		if (value === '' || value === null) return '';
		return control(s).check(s, value);
	}

	// Zone, item or reward code names under a list, e.g. [1000 Venture Explorer] [1100 Avant Gardens]
	function listNames(s, value) {
		if (!s.listOf || s.listOf === 'number' || !value) return '<div class="setting-names" data-names></div>';
		var names = state.names[keyOf(s)] || {};
		return '<div class="setting-names" data-names>' + value.split(',').filter(Boolean).map(function (id) {
			return '<span class="badge text-bg-secondary">' + esc(id) + (names[id] ? ' &middot; ' + esc(names[id]) : ' &middot; <em>unknown</em>') + '</span>';
		}).join('') + '</div>';
	}

	function refreshNames(s, input) {
		var value = control(s).read(input, s);
		if (!s.listOf || s.listOf === 'number' || !value) return;
		api.get('/api/settings/names?of=' + s.listOf + '&ids=' + encodeURIComponent(value)).then(function (d) {
			if (!d.success) return;
			state.names[keyOf(s)] = d.names;
			var box = input.parentElement.querySelector('[data-names]');
			if (box) box.outerHTML = listNames(s, value);
		});
	}

	// ---------------------------------------------------------------- rendering

	function sourceText(s) {
		var fileLabel = '<code>' + esc(s.file) + '</code>';
		switch (s.source) {
		case 'web': return 'Set here' + (s.updatedBy ? ' by ' + esc(s.updatedBy) + ' ' + esc(fmt.unix(s.updatedAt)) : '') + (s.webWins ? ', overriding the ' + (s.fileSource === 'env' ? 'environment' : 'file') : '');
		case 'env': return 'From the <code>' + esc(s.env) + '</code> environment variable';
		case 'file': return 'From ' + fileLabel;
		default: return 'Default' + (s['default'] !== '' && s.type !== 'secret' ? ': ' + esc(shownValue(s, s['default'])) : '');
		}
	}

	function shownValue(s, value) {
		if (s.type === 'bool') return value === '1' ? 'on' : 'off';
		if (value === '' || value === undefined || value === null) return '(empty)';
		if (s.type === 'choice') return choiceLabel(s, String(value));
		return String(value) + (s.unit ? ' ' + s.unit : '');
	}

	// Read-only: the database connection and the dashboard's keys can't come from the database
	function lockedControl(s) {
		var value = s.secret ? (s.isSet ? 'set' : 'not set') : shownValue(s, s.value);
		return '<div class="setting-locked"><div><span aria-hidden="true">&#128274;</span> <span class="value">' + esc(value) + '</span></div>' +
			'<div class="small text-body-secondary mt-1">Change it in <code>' + esc(s.file) + '</code> or the <code>' + esc(s.env) + '</code> environment variable, then restart. ' +
			(s.name.indexOf('mysql_') === 0 || s.name.indexOf('sqlite_') === 0 || s.name === 'database_type'
				? 'The servers need it to reach the database, so they can\'t read it from the database.'
				: 'The dashboard needs it before it can read settings from the database.') + '</div></div>';
	}

	function settingHtml(s, compact) {
		var key = keyOf(s), value = currentValue(s), pending = state.pending[key];
		var classes = 'setting' + (pending ? ' is-changed' : '') + (state.errors[key] ? ' has-error' : '');
		var tags = (s.restart ? '<span class="setting-tag restart" title="Only read when a server starts">restart</span>' : '') +
			(s.unused ? '<span class="setting-tag unused" title="In the shipped .ini, but this version doesn\'t read it">unused</span>' : '') +
			(!s.known ? '<span class="setting-tag unused">custom</span>' : '');
		var label = '<label class="setting-title" for="' + domId(key) + '">' + esc(s.title) + '</label>' + tags;
		// Anything ever changed here has a history (and undo)
		var history = s.updatedAt ? ' &middot; <a href="/settings/history?file=' + encodeURIComponent(s.file) + '&amp;name=' + encodeURIComponent(s.name) + '">History</a>' : '';
		var meta = '<div class="setting-meta"><code>' + esc(s.name) + '</code> &middot; ' + sourceText(s) + history + '</div>';

		var notes = '';
		if (pending && pending.value === null) notes += '<div class="setting-note text-info-emphasis">The value set here will be removed; ' + esc(shownValue(s, fallbackValue(s))) + ' applies again.</div>';
		var fileSets = s.fileSource === 'file' || s.fileSource === 'env';
		if (pending && pending.value !== null && fileSets) {
			notes += '<div class="form-check setting-note mb-0"><input class="form-check-input" type="checkbox" data-wins="' + esc(key) + '" id="wins-' + domId(key) + '"' + (pending.webWins ? ' checked' : '') + '>' +
				'<label class="form-check-label" for="wins-' + domId(key) + '">Override the value from the ' + (s.fileSource === 'env' ? 'environment' : 'file') + '</label></div>';
		} else if (!pending && s.hasWebValue && !s.webWins && fileSets && s.source !== 'web') {
			notes += '<div class="setting-note text-warning-emphasis">A value set here is ignored because the ' + (s.fileSource === 'env' ? 'environment' : 'file') + ' sets this.</div>';
		}
		var actions = '';
		if (pending) actions += '<button type="button" class="btn btn-link btn-sm p-0 me-3" data-undo="' + esc(key) + '">Undo</button>';
		else if (s.hasWebValue && !s.fileOnly) actions += '<button type="button" class="btn btn-link btn-sm p-0" data-clear="' + esc(key) + '">Remove the value set here</button>';

		var input = s.fileOnly ? lockedControl(s) : control(s).render(s, value);
		var error = '<div class="invalid-feedback d-block small" data-error="' + esc(key) + '">' + esc(state.errors[key] || '') + '</div>';

		if (compact) {
			return '<div class="' + classes + '" data-setting="' + esc(key) + '"><label class="form-label small mb-1" for="' + domId(key) + '">' + esc(s.title) + '</label>' +
				input + error + (actions ? '<div>' + actions + '</div>' : '') + '</div>';
		}
		return '<div class="' + classes + '" data-setting="' + esc(key) + '">' +
			'<div>' + label + (s.description ? '<p class="setting-description">' + esc(s.description) + '</p>' : '') + meta + '</div>' +
			'<div>' + input + error + notes + (actions ? '<div class="mt-1">' + actions + '</div>' : '') + '</div></div>';
	}

	function sectionHtml(section, settings, heading) {
		var title = heading ? esc(heading) + ' &rsaquo; ' + esc(section.name) : esc(section.name);
		if (!conditionMet(section.condition) && !state.query) {
			return '<div class="card mb-3 settings-section-off"><div class="card-body py-2"><strong>' + title + '</strong> &middot; used when ' + describeCondition(section.condition) + '</div></div>';
		}
		var shown = settings.filter(function (s) { return conditionMet(s.condition) || state.query; });
		var hidden = settings.length - shown.length;
		var body;
		if (section.layout === 'grid') {
			body = '<div class="setting-grid">' + shown.map(function (s) { return settingHtml(s, true); }).join('') + '</div>';
		} else if (section.layout === 'pairs') {
			var pairs = [];
			for (var i = 0; i < shown.length; i += 2) pairs.push('<div class="setting-pair">' + settingHtml(shown[i], true) + (shown[i + 1] ? settingHtml(shown[i + 1], true) : '') + '</div>');
			body = pairs.join('');
		} else {
			body = shown.map(function (s) { return settingHtml(s, false); }).join('');
		}
		var changed = settings.filter(isChanged).length;
		return '<section class="card mb-3" aria-label="' + esc(section.name) + '">' +
			'<div class="card-header"><div class="d-flex justify-content-between align-items-baseline gap-2"><h3 class="h6 mb-0">' + title + '</h3>' +
			(changed ? '<span class="small text-body-secondary">' + changed + ' changed from default</span>' : '') + '</div>' +
			(section.description ? '<div class="small text-body-secondary mt-1">' + esc(section.description) + '</div>' : '') + '</div>' +
			'<div class="card-body p-0">' + body + '</div>' +
			(hidden ? '<div class="card-footer small text-body-secondary">' + hidden + ' more setting' + (hidden === 1 ? '' : 's') + ' appear when they apply.</div>' : '') +
			'</section>';
	}

	function visibleByCategory() {
		var byCategory = {};
		state.settings.forEach(function (s) {
			if (!matchesFilters(s)) return;
			(byCategory[s.category] = byCategory[s.category] || []).push(s);
		});
		return byCategory;
	}

	function renderNav(byCategory) {
		el.nav.innerHTML = state.categories.filter(function (c) {
			return c.id !== 'other' || state.settings.some(function (s) { return s.category === 'other'; });
		}).map(function (c) {
			var count = (byCategory[c.id] || []).length;
			var pending = state.settings.filter(function (s) { return s.category === c.id && isPending(s); }).length;
			var active = !state.query && c.id === state.category;
			return '<a href="#' + esc(c.id) + '" class="list-group-item list-group-item-action' + (active ? ' active' : '') + (count ? '' : ' disabled') + '" data-category="' + esc(c.id) + '">' +
				'<span>' + esc(c.name) + (pending ? ' <span class="unsaved-dot" title="' + pending + ' unsaved change' + (pending === 1 ? '' : 's') + '"></span>' : '') + '</span><span class="count">' + count + '</span></a>';
		}).join('');
		// On phones the categories are one scrolling row: keep the current one in view
		var active = el.nav.querySelector('.active');
		if (active && el.nav.scrollWidth > el.nav.clientWidth) active.scrollIntoView({ block: 'nearest', inline: 'center' });
	}

	function render() {
		var byCategory = visibleByCategory();
		renderNav(byCategory);
		var categories = state.query ? state.categories.filter(function (c) { return byCategory[c.id]; }) : state.categories.filter(function (c) { return c.id === state.category; });
		if (!categories.length || !categories.some(function (c) { return byCategory[c.id]; })) {
			el.body.innerHTML = '<p class="text-body-secondary">No settings match.</p>';
		} else {
			el.body.innerHTML = categories.map(function (c) {
				var settings = byCategory[c.id] || [];
				var sections = state.sections.filter(function (sec) { return sec.category === c.id; });
				return '<div class="mb-4">' + (state.query ? '<h2 class="h5 mb-2">' + esc(c.name) + '</h2>' : '<p class="text-body-secondary">' + esc(c.description) + '</p>') +
					sections.map(function (sec) {
						var inSection = settings.filter(function (s) { return s.section === sec.name; });
						return inSection.length ? sectionHtml(sec, inSection) : '';
					}).join('') + '</div>';
			}).join('');
		}
		renderSaveBar();
		if (state.openPicker) {
			var details = el.body.querySelector('[data-picker="' + CSS.escape(state.openPicker.key) + '"]');
			if (details) {
				details.open = true;
				details.querySelector('[data-pick-filter]').value = state.openPicker.filter;
				fillPicker(details);
			}
			state.openPicker = null;
		}
	}

	function renderSaveBar() {
		var count = Object.keys(state.pending).length, errors = Object.keys(state.errors);
		el.saveBar.classList.toggle('d-none', count === 0);
		// An invalid value may be off-screen or in another category: say which, and jump to it
		el.saveCount.innerHTML = esc(count + ' unsaved change' + (count === 1 ? '' : 's')) + (errors.length
			? ' &middot; <a href="#" class="link-danger" data-goto="' + esc(errors[0]) + '">' + (errors.length === 1 ? esc(state.byKey[errors[0]].title) + ' needs fixing' : errors.length + ' need fixing') + '</a>'
			: '');
		el.save.disabled = state.saving || errors.length > 0;
	}

	// Update one setting's row in place while typing (re-rendering would lose focus)
	function refreshRow(key) {
		var row = el.body.querySelector('[data-setting="' + CSS.escape(key) + '"]');
		if (!row) return;
		row.classList.toggle('is-changed', isPending(state.byKey[key]));
		row.classList.toggle('has-error', !!state.errors[key]);
		var error = row.querySelector('[data-error]');
		if (error) error.textContent = state.errors[key] || '';
		renderNav(visibleByCategory());
		renderSaveBar();
	}

	// ---------------------------------------------------------------- editing

	function edit(key, value) {
		var s = state.byKey[key];
		var original = s.type === 'secret' ? '' : (s.value === undefined ? '' : String(s.value));
		if (value === original) {
			delete state.pending[key];
		} else {
			var fileSets = s.fileSource === 'file' || s.fileSource === 'env';
			var before = state.pending[key];
			state.pending[key] = { value: value, webWins: before ? before.webWins : fileSets };
		}
		var error = checkValue(s, value);
		if (error) state.errors[key] = error; else delete state.errors[key];
	}

	function onInput(input, finished) {
		var key = input.dataset.input, s = state.byKey[key];
		var hadPending = isPending(s);
		edit(key, control(s).read(input, s));
		if (s.type === 'bool') input.nextElementSibling.textContent = input.checked ? 'On' : 'Off';
		// A setting others depend on, or the first/last edit on a row (notes change): redraw
		if (state.controllers[key] || (finished && hadPending !== isPending(s))) {
			render();
			var again = document.getElementById(domId(key));
			if (again && !finished) again.focus();
		} else {
			refreshRow(key);
		}
		if (finished && s.type === 'int_list') refreshNames(s, input);
	}

	function save() {
		var keys = Object.keys(state.pending);
		if (!keys.length || state.saving) return;
		state.saving = true;
		renderSaveBar();
		var changes = keys.map(function (key) {
			var s = state.byKey[key], p = state.pending[key];
			return { file: s.file, name: s.name, value: p.value, webWins: p.value !== null && p.webWins };
		});
		api.post('/api/settings/batch', { changes: changes }).then(function (d) {
			state.saving = false;
			if (!d.success) {
				state.errors = d.errors || {};
				toast(d.error || 'Could not save', 'danger');
				render();
				return;
			}
			toast(d.message, 'success');
			if (d.restart && d.restart.length) {
				el.restartList.textContent = d.restart.join(', ');
				el.restartNotice.classList.remove('d-none');
			}
			state.pending = {};
			state.errors = {};
			load();
		}).catch(function () { state.saving = false; renderSaveBar(); });
	}

	function discard() {
		state.pending = {};
		state.errors = {};
		render();
	}

	function goTo(key) {
		var s = state.byKey[key];
		if (!s) return;
		state.query = '';
		el.search.value = '';
		state.category = s.category;
		render();
		var row = el.body.querySelector('[data-setting="' + CSS.escape(key) + '"]');
		if (row) { row.scrollIntoView({ block: 'center' }); var input = document.getElementById(domId(key)); if (input) input.focus(); }
	}

	// ---------------------------------------------------------------- loading

	function load() {
		return api.get('/api/settings').then(function (d) {
			if (!d.success) return toast(d.error || 'Could not load settings', 'danger');
			state.categories = d.categories;
			state.sections = d.sections;
			state.settings = d.settings;
			state.files = d.files;
			state.names = d.names || {};
			state.byKey = {};
			state.controllers = {};
			d.settings.forEach(function (s) {
				state.byKey[keyOf(s)] = s;
				if (s.condition) state.controllers[s.condition.file + '/' + s.condition.key] = true;
			});
			d.sections.forEach(function (sec) { if (sec.condition) state.controllers[sec.condition.file + '/' + sec.condition.key] = true; });
			// Drop edits to settings that no longer exist
			Object.keys(state.pending).forEach(function (key) { if (!state.byKey[key]) delete state.pending[key]; });
			// #<category>, or #<file>/<name> for one setting (the UGC page's "Open in Settings" links)
			var fromHash = decodeURIComponent(location.hash.slice(1)), linked = !state.category && state.byKey[fromHash];
			if (!state.category) state.category = state.categories.some(function (c) { return c.id === fromHash; }) ? fromHash : linked ? linked.category : state.categories[0].id;
			document.getElementById('addFile').innerHTML = d.files.map(function (f) { return '<option value="' + esc(f.file) + '">' + esc(f.name) + ' (' + esc(f.file) + ')</option>'; }).join('');
			render();
			if (linked) goTo(fromHash);
		});
	}

	// ---------------------------------------------------------------- events

	el.body.addEventListener('input', function (e) { if (e.target.matches('[data-input]')) onInput(e.target, false); });
	el.body.addEventListener('change', function (e) {
		if (e.target.matches('[data-input]')) onInput(e.target, true);
		if (e.target.matches('[data-wins]')) { var p = state.pending[e.target.dataset.wins]; if (p) p.webWins = e.target.checked; }
	});
	el.body.addEventListener('toggle', function (e) { if (e.target.matches('[data-picker]') && e.target.open) fillPicker(e.target); }, true);
	el.body.addEventListener('input', function (e) { if (e.target.matches('[data-pick-filter]')) fillPicker(e.target.closest('[data-picker]')); });
	el.body.addEventListener('change', function (e) { if (e.target.matches('[data-pick-toggle]')) pick(e.target.dataset.pickToggle, e.target.value, e.target.checked); });
	el.body.addEventListener('click', function (e) {
		var remove = e.target.closest('[data-pick-remove]');
		if (remove) { pick(remove.dataset.pickRemove, remove.dataset.id, false); return; }
		var undo = e.target.closest('[data-undo]'), clear = e.target.closest('[data-clear]'), go = e.target.closest('[data-goto]');
		if (undo) { delete state.pending[undo.dataset.undo]; delete state.errors[undo.dataset.undo]; render(); }
		if (clear) { state.pending[clear.dataset.clear] = { value: null, webWins: false }; render(); }
		if (go) { e.preventDefault(); goTo(go.dataset.goto); }
	});
	el.nav.addEventListener('click', function (e) {
		var item = e.target.closest('[data-category]');
		if (!item) return;
		e.preventDefault();
		state.category = item.dataset.category;
		state.query = '';
		el.search.value = '';
		try { history.replaceState(null, '', '#' + state.category); } catch (err) {}
		render();
		window.scrollTo({ top: 0 });
	});
	el.search.addEventListener('input', function () { state.query = el.search.value.trim().toLowerCase(); render(); });
	el.filter.addEventListener('change', function (e) { state.filter = e.target.value; render(); });
	el.showUnused.addEventListener('change', render);
	el.save.addEventListener('click', save);
	el.saveCount.addEventListener('click', function (e) {
		var go = e.target.closest('[data-goto]');
		if (go) { e.preventDefault(); goTo(go.dataset.goto); }
	});
	el.discard.addEventListener('click', discard);
	document.addEventListener('keydown', function (e) {
		if ((e.ctrlKey || e.metaKey) && e.key === 's' && Object.keys(state.pending).length) { e.preventDefault(); save(); }
	});
	window.addEventListener('beforeunload', function (e) {
		if (Object.keys(state.pending).length) { e.preventDefault(); e.returnValue = ''; }
	});

	// Custom settings (ones this page doesn't know)
	var addModal = new bootstrap.Modal(document.getElementById('addModal'));
	document.getElementById('addSetting').addEventListener('click', function () { addModal.show(); });
	document.getElementById('addForm').addEventListener('submit', function (e) {
		e.preventDefault();
		api.action('/api/settings', { file: document.getElementById('addFile').value, name: document.getElementById('addName').value.trim(),
			value: document.getElementById('addValue').value }).then(function (d) { toast(d.message, 'success'); addModal.hide(); state.category = 'other'; load(); }).catch(function () {});
	});

	// Someone else saving: reload unless this user has unsaved edits
	if (window.Live) Live.on('settings', Live.throttle(function () { if (!Object.keys(state.pending).length) load(); }, 1000));

	load();
})();
