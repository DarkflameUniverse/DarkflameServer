/**
 * The CDClient Browser page. Everything is addressed by the hash, so views can be linked and the back button works:
 *   #/                       search and the list of tables
 *   #/search/<text>          search results
 *   #/table/<Name>?...       a table's rows (start, order, dir, q, filters as JSON [{column, op, value}])
 *   #/<link>/<id>            object, mission, skill, behavior, activity, zone, loot_matrix, loot_table
 * Which columns link where comes from /api/cdclient/schema; links without their own view open the target table
 * filtered to that value.
 */
(function () {
	'use strict';

	var VIEWS = ['object', 'mission', 'skill', 'behavior', 'activity', 'zone', 'loot_matrix', 'loot_table'];
	var PAGE = 50;
	var nf = new Intl.NumberFormat();
	var view = document.getElementById('view');
	var schema = null;
	var viewer = null;

	function api404(d) { return d && d.success === false; }

	// ---- links ----

	function hashFor(link, id) {
		if (VIEWS.indexOf(link) !== -1) return '#/' + link + '/' + id;
		var target = schema && schema.links[link];
		if (!target) return null;
		return tableHash(target.table, { filters: [{ column: target.column, op: '=', value: String(id) }] });
	}

	function tableHash(table, params) {
		var query = [];
		Object.keys(params || {}).forEach(function (k) {
			var v = params[k];
			if (v === undefined || v === null || v === '' || (Array.isArray(v) && !v.length)) return;
			query.push(k + '=' + encodeURIComponent(Array.isArray(v) ? JSON.stringify(v) : v));
		});
		return '#/table/' + encodeURIComponent(table) + (query.length ? '?' + query.join('&') : '');
	}

	function refLink(ref) {
		var href = hashFor(ref.link, ref.id);
		var text = esc(ref.name) + ' <span class="text-body-secondary">' + esc(ref.id) + '</span>';
		return href ? '<a href="' + href + '">' + text + '</a>' : text;
	}

	function chips(refs) {
		if (!refs || !refs.length) return '<span class="text-body-secondary">None</span>';
		return refs.map(function (r) { return '<span class="cdc-chip badge bg-body-tertiary text-body border">' + refLink(r) + '</span>'; }).join('');
	}

	function componentTable(type) {
		var match = schema.componentTypes.filter(function (c) { return c.type === type; })[0];
		return match ? match : null;
	}

	// One cell: linked when its column points somewhere
	function cell(value, column, row, table) {
		if (value === null || value === undefined) return '<span class="cdc-null">NULL</span>';
		// REAL columns hold 32-bit floats: show the digits they actually have (0.4, not 0.4000000059604645)
		var text = esc(column && column.type === 'REAL' && typeof value === 'number' ? +value.toPrecision(7) : value);
		if (column && column.link && value !== 0 && value !== -1) {
			var href = hashFor(column.link, value);
			if (href) return '<a href="' + href + '">' + text + '</a>';
		}
		// ComponentsRegistry: the type by name, and the component's row in its table
		if (table === 'ComponentsRegistry' && column) {
			if (column.name === 'component_type') {
				var type = componentTable(value);
				return text + (type ? ' <span class="text-body-secondary">' + esc(type.name) + '</span>' : '');
			}
			if (column.name === 'component_id') {
				var owner = componentTable(row.component_type);
				if (owner && owner.table) return '<a href="' + tableHash(owner.table, { filters: [{ column: 'id', op: '=', value: String(value) }] }) + '">' + text + '</a>';
			}
		}
		return text;
	}

	function columnsOf(table) {
		var t = schema.tables.filter(function (x) { return x.name === table; })[0];
		return t ? t.columns : [];
	}

	// A row as a two-column table
	function keyValues(row, columns, table) {
		columns = columns || Object.keys(row).map(function (k) { return { name: k }; });
		return '<table class="table table-sm cdc-kv mb-0"><tbody>' + columns.map(function (c) {
			return '<tr><th>' + esc(c.name) + (c.link ? ' <span class="badge bg-body-tertiary text-body border">' + esc(c.link) + '</span>' : '') + '</th><td>' + cell(row[c.name], c, row, table) + '</td></tr>';
		}).join('') + '</tbody></table>';
	}

	// Rows as a table, with the schema's links
	function rowsTable(rows, table, columns) {
		if (!rows.length) return '<div class="text-body-secondary small">No rows.</div>';
		columns = columns || (table ? columnsOf(table) : null) || [];
		if (!columns.length) columns = Object.keys(rows[0]).map(function (k) { return { name: k }; });
		return '<div class="cdc-rows-wrap"><table class="table table-sm table-hover cdc-rows mb-0"><thead><tr>' +
			columns.map(function (c) { return '<th>' + esc(c.name) + '</th>'; }).join('') + '</tr></thead><tbody>' +
			rows.map(function (row) {
				return '<tr>' + columns.map(function (c) { return '<td>' + cell(row[c.name], c, row, table) + '</td>'; }).join('') + '</tr>';
			}).join('') + '</tbody></table></div>';
	}

	function card(title, body, extra) {
		return '<div class="card mb-3"><div class="card-header d-flex justify-content-between align-items-center"><h5 class="mb-0 fs-6">' + title + '</h5>' +
			(extra || '') + '</div><div class="card-body">' + body + '</div></div>';
	}

	function localized(texts) {
		var keys = Object.keys(texts || {});
		if (!keys.length) return '';
		return card('Texts (locale)', '<table class="table table-sm cdc-kv mb-0"><tbody>' + keys.map(function (k) {
			return '<tr><th>' + esc(k) + '</th><td style="font-family: inherit">' + esc(texts[k]) + '</td></tr>';
		}).join('') + '</tbody></table>');
	}

	// "Used by": everything pointing here, per table and column
	function usedBy(references, link, id) {
		if (!references || !references.length) return card('Used by', '<span class="text-body-secondary">Nothing in the CDClient points here.</span>');
		return card('Used by', references.map(function (r) {
			var href = tableHash(r.table, { filters: [{ column: r.column, op: '=', value: String(id) }] });
			return '<details class="mb-2"><summary><a href="' + href + '">' + esc(r.table) + '.' + esc(r.column) + '</a> <span class="badge text-bg-secondary">' + nf.format(r.count) + '</span>' +
				(r.objects && r.objects.length ? ' <span class="small text-body-secondary">on ' + r.objects.length + ' object(s)</span>' : '') + '</summary>' +
				(r.objects && r.objects.length ? '<div class="my-2">' + chips(r.objects) + '</div>' : '') +
				rowsTable(r.rows, r.table) + (r.count > r.rows.length ? '<div class="small text-body-secondary">First ' + r.rows.length + ' of ' + nf.format(r.count) + '; <a href="' + href + '">see all</a>.</div>' : '') +
				'</details>';
		}).join(''));
	}

	function header(title, subtitle) {
		return '<div class="mb-3"><h3 class="mb-0">' + title + '</h3>' + (subtitle ? '<div class="text-body-secondary small">' + subtitle + '</div>' : '') + '</div>';
	}

	function showError(message) {
		view.innerHTML = '<div class="alert alert-warning">' + esc(message) + '</div>';
	}

	function disposeViewer() {
		if (viewer) { viewer.dispose(); viewer = null; }
	}

	// ---- views ----

	function home() {
		var tables = schema.tables;
		view.innerHTML = card('Tables <span class="badge text-bg-secondary">' + tables.length + '</span>',
			'<input class="form-control form-control-sm mb-2" id="tableFilter" placeholder="Filter tables" aria-label="Filter tables">' +
			'<div class="cdc-tables" id="tableLinks">' + tables.map(function (t) {
				return '<a href="' + tableHash(t.name) + '" data-name="' + esc(t.name.toLowerCase()) + '">' + esc(t.name) + '</a>';
			}).join('') + '</div>');
		document.getElementById('tableFilter').addEventListener('input', function (e) {
			var text = e.target.value.trim().toLowerCase();
			document.querySelectorAll('#tableLinks a').forEach(function (a) { a.classList.toggle('d-none', text && a.dataset.name.indexOf(text) === -1); });
		});
	}

	function search(text) {
		document.getElementById('searchInput').value = text;
		api.get('/api/cdclient/search?q=' + encodeURIComponent(text)).then(function (d) {
			if (api404(d)) return showError(d.error);
			var sections = [['Exact ID', d.exact], ['Objects', d.objects], ['Missions', d.missions], ['Skills', d.skills], ['Activities', d.activities], ['Zones', d.zones]];
			var found = sections.filter(function (s) { return s[1].length; });
			if (found.length === 1 && found[0][1].length === 1) { window.location.hash = hashFor(found[0][1][0].link, found[0][1][0].id); return; }
			view.innerHTML = header('Results for "' + esc(text) + '"') + (found.length ? found.map(function (s) {
				return card(s[0], '<div class="list-group list-group-flush">' + s[1].map(function (r) {
					return '<div class="list-group-item px-0"><span class="badge bg-body-tertiary text-body border me-2">' + esc(r.link) + '</span>' + (r.link === 'object' ? item(r) : refLink(r)) + '</div>';
				}).join('') + '</div>');
			}).join('') : '<div class="text-body-secondary">Nothing found.</div>');
		});
	}

	var OPS = ['=', '!=', '<', '<=', '>', '>=', 'contains', 'starts', 'null', 'notnull'];

	function tableView(name, params) {
		var start = parseInt(params.start || '0', 10) || 0;
		var filters = [];
		try { filters = params.filters ? JSON.parse(params.filters) : []; } catch (e) { filters = []; }
		var url = '/api/cdclient/tables/' + encodeURIComponent(name) + '/rows?start=' + start + '&length=' + PAGE +
			(params.order ? '&order=' + encodeURIComponent(params.order) + '&dir=' + (params.dir || 'asc') : '') +
			(params.q ? '&q=' + encodeURIComponent(params.q) : '') + (filters.length ? '&filters=' + encodeURIComponent(JSON.stringify(filters)) : '');
		api.get(url).then(function (d) {
			if (api404(d)) return showError(d.error);
			var state = { filters: filters, q: params.q || '', order: params.order || '', dir: params.dir || 'asc' };
			var go = function (changes) { window.location.hash = tableHash(name, Object.assign({ q: state.q, filters: state.filters, order: state.order, dir: state.order ? state.dir : '', start: 0 }, changes)); };
			var columnOptions = d.columns.map(function (c) { return '<option>' + esc(c.name) + '</option>'; }).join('');
			var filterChips = filters.map(function (f, i) {
				return '<span class="badge text-bg-primary me-1">' + esc(f.column) + ' ' + esc(f.op) + ' ' + esc(f.value || '') +
					' <button type="button" class="btn-close btn-close-white ms-1" style="font-size:.5rem" data-remove="' + i + '" aria-label="Remove filter"></button></span>';
			}).join('');
			var pages = Math.max(1, Math.ceil(d.total / PAGE)), page = Math.floor(start / PAGE) + 1;
			var head = '<tr>' + d.columns.map(function (c) {
				var arrow = state.order === c.name ? (state.dir === 'desc' ? ' &darr;' : ' &uarr;') : '';
				return '<th><a href="#" class="text-body text-decoration-none" data-order="' + esc(c.name) + '" title="' + esc(c.type) + (c.link ? ', links to ' + esc(c.link) : '') + '">' + esc(c.name) + arrow + '</a></th>';
			}).join('') + '</tr>';
			view.innerHTML = header(esc(name), nf.format(d.total) + ' matching rows') +
				'<div class="card mb-3"><div class="card-body">' +
				'<form class="row g-2 align-items-end mb-2" id="filterForm">' +
				'<div class="col-sm-3"><label class="form-label small mb-1" for="fColumn">Column</label><select class="form-select form-select-sm" id="fColumn">' + columnOptions + '</select></div>' +
				'<div class="col-sm-2"><label class="form-label small mb-1" for="fOp">Is</label><select class="form-select form-select-sm" id="fOp">' + OPS.map(function (o) { return '<option>' + o + '</option>'; }).join('') + '</select></div>' +
				'<div class="col-sm-3"><label class="form-label small mb-1" for="fValue">Value</label><input class="form-control form-control-sm" id="fValue"></div>' +
				'<div class="col-sm-1"><button class="btn btn-sm btn-outline-primary w-100">Add</button></div>' +
				'<div class="col-sm-3"><label class="form-label small mb-1" for="fSearch">Any column</label><input class="form-control form-control-sm" id="fSearch" value="' + esc(state.q) + '" placeholder="Search, then Enter"></div>' +
				'</form><div>' + filterChips + '</div></div></div>' +
				'<div class="card"><div class="card-body p-0 cdc-rows-wrap"><table class="table table-sm table-hover cdc-rows mb-0"><thead>' + head + '</thead><tbody>' +
				d.rows.map(function (row) { return '<tr>' + d.columns.map(function (c) { return '<td>' + cell(row[c.name], c, row, name) + '</td>'; }).join('') + '</tr>'; }).join('') +
				'</tbody></table></div><div class="card-footer d-flex align-items-center gap-2 small">' +
				'<button class="btn btn-sm btn-outline-secondary" id="prevPage"' + (page <= 1 ? ' disabled' : '') + '>&larr;</button>' +
				'<span>Page ' + page + ' of ' + nf.format(pages) + '</span>' +
				'<button class="btn btn-sm btn-outline-secondary" id="nextPage"' + (page >= pages ? ' disabled' : '') + '>&rarr;</button></div></div>';

			document.getElementById('filterForm').addEventListener('submit', function (e) {
				e.preventDefault();
				var search = document.getElementById('fSearch');
				if (document.activeElement === search) return go({ q: search.value.trim() });
				var filter = { column: document.getElementById('fColumn').value, op: document.getElementById('fOp').value, value: document.getElementById('fValue').value };
				go({ filters: state.filters.concat([filter]) });
			});
			view.querySelectorAll('[data-remove]').forEach(function (b) {
				b.addEventListener('click', function () { go({ filters: state.filters.filter(function (f, i) { return i !== parseInt(b.dataset.remove, 10); }) }); });
			});
			view.querySelectorAll('[data-order]').forEach(function (a) {
				a.addEventListener('click', function (e) {
					e.preventDefault();
					var column = a.dataset.order;
					go({ order: column, dir: state.order === column && state.dir === 'asc' ? 'desc' : 'asc' });
				});
			});
			document.getElementById('prevPage').addEventListener('click', function () { go({ start: Math.max(0, start - PAGE) }); });
			document.getElementById('nextPage').addEventListener('click', function () { go({ start: start + PAGE }); });
		});
	}

	// ---- game values ----

	// A chance as a percentage with the digits that matter (0.4%, 12.5%, 100%)
	function pct(chance) {
		var p = (chance || 0) * 100;
		if (p === 0) return '0%';
		if (p >= 99.95) return '100%';
		return (p >= 10 ? p.toFixed(1) : p >= 1 ? p.toFixed(2) : +p.toPrecision(2)) + '%';
	}

	function num(value) {
		return typeof value === 'number' ? nf.format(+value.toPrecision(7)) : esc(value === null || value === undefined ? '' : value);
	}

	// Rarity as the game shows it: 1 to 4 stars, coloured
	function rarity(value) {
		if (!value || value < 1) return '';
		return ' <span class="rarity-' + esc(value) + '" title="Rarity ' + esc(value) + '" aria-label="Rarity ' + esc(value) + '">' + '\u2605'.repeat(Math.min(value, 5)) + '</span>';
	}

	function icon(lot) {
		return '<img class="cdc-icon" src="/api/icon/' + esc(lot) + '" alt="" loading="lazy" onerror="this.remove()">';
	}

	function iconById(id, size) {
		if (!id || id <= 0) return '';
		return '<img src="/api/icon_id/' + esc(id) + '" alt="" width="' + (size || 20) + '" height="' + (size || 20) + '" class="me-1 rounded" loading="lazy" onerror="this.remove()">';
	}

	// An object with its icon and rarity; other links as they are
	function item(ref) {
		if (!ref) return '<span class="text-body-secondary">-</span>';
		if (ref.link !== 'object') return ref.link ? refLink(ref) : '<span class="font-monospace">' + esc(ref.id) + '</span>';
		return '<a href="' + hashFor('object', ref.id) + '">' + icon(ref.id) + esc(ref.name) + '</a>' + rarity(ref.rarity) + ' <span class="text-body-secondary small">' + esc(ref.id) + '</span>';
	}

	function items(refs, count) {
		if (!refs || !refs.length) return '<span class="text-body-secondary">None</span>';
		return refs.map(function (r) { return '<div>' + item(r.item || r) + (count && r.count !== undefined ? ' &times; ' + num(r.count) : '') + '</div>'; }).join('');
	}

	function none(text) { return '<span class="text-body-secondary">' + (text || 'None') + '</span>'; }

	function raw(title, body) {
		return '<details class="mt-2"><summary class="small text-body-secondary">' + title + '</summary><div class="mt-2">' + body + '</div></details>';
	}

	// Like raw, but the body is only built when opened (loot lists can run to thousands of rows)
	var lazy = {}, lazyCount = 0;
	function later(title, render) {
		lazy[++lazyCount] = render;
		return '<details class="mt-2" data-lazy="' + lazyCount + '"><summary class="small text-body-secondary">' + title + '</summary><div class="mt-2"></div></details>';
	}

	view.addEventListener('toggle', function (e) {
		var details = e.target;
		if (!details.open || !details.dataset || !lazy[details.dataset.lazy]) return;
		details.lastElementChild.innerHTML = lazy[details.dataset.lazy]();
		delete lazy[details.dataset.lazy];
	}, true);

	function table(head, rows, cls) {
		return '<div class="cdc-rows-wrap"><table class="table table-sm align-middle mb-0 ' + (cls || '') + '"><thead><tr>' + head.map(function (h) {
			return '<th' + (h.num ? ' class="num"' : '') + '>' + h.text + '</th>';
		}).join('') + '</tr></thead><tbody>' + rows.join('') + '</tbody></table></div>';
	}

	function stat(label, value) {
		return '<div class="col"><div class="small text-body-secondary">' + label + '</div><div class="cdc-stat">' + value + '</div></div>';
	}

	// Mission tasks that count something
	function countedBy(tasks) {
		if (!tasks || !tasks.length) return '';
		return card('Counted by mission tasks', tasks.map(function (t) {
			return '<div>' + refLink(t.mission) + ' <span class="badge bg-body-tertiary text-body border">' + esc(t.taskType) + '</span>' +
				(t.targetValue ? ' <span class="small text-body-secondary">&times; ' + num(t.targetValue) + '</span>' : '') + '</div>';
		}).join(''));
	}

	/**
	 * A loot matrix as the server rolls it: each item's chance of dropping at least once and how many on average, and
	 * under that how each entry rolls. `per` names what one roll is (a smash, an opened package, an activity reward).
	 */
	function lootOdds(odds, per) {
		if (!odds || !odds.entries || !odds.entries.length) return none('No loot matrix rows');
		var rows = odds.items.map(function (i) {
			return '<tr><td>' + item(i.item) + '</td><td class="num">' + pct(i.chance) + '</td><td class="num">' + num(+i.expected.toPrecision(3)) + '</td></tr>';
		});
		var shown = rows.length > 25 ? rows.slice(0, 25) : rows;
		var html = table([{ text: 'Item' }, { text: 'Chance per ' + esc(per), num: true }, { text: 'Average count', num: true }], shown, 'cdc-odds');
		if (rows.length > shown.length) html += later('All ' + rows.length + ' items', function () { return table([{ text: 'Item' }, { text: 'Chance', num: true }, { text: 'Average', num: true }], rows, 'cdc-odds'); });
		html += later('How it rolls (' + odds.entries.length + (odds.entries.length === 1 ? ' entry' : ' entries') + ')', function () { return odds.entries.map(function (e) {
			return '<div class="mb-3"><div><a href="' + hashFor('loot_table', e.lootTable) + '">Loot table ' + esc(e.lootTable) + '</a>: rolls with ' + pct(e.percent) + ', then ' +
				(e.minToDrop === e.maxToDrop ? esc(e.minToDrop) : esc(e.minToDrop) + '&ndash;' + esc(e.maxToDrop)) + ' drop(s)' +
				(e.flagID ? ' <span class="small text-body-secondary">(flagID ' + esc(e.flagID) + ' in the row; the server\'s roll doesn\'t check it)</span>' : '') + '</div>' +
				'<div class="small text-body-secondary">Each drop rolls a rarity (<a href="' + hashFor('rarity_table', e.rarityTable) + '">rarity table ' + esc(e.rarityTable) + '</a>): ' +
				e.rolledRarity.map(function (r) { return rarity(r.rarity).trim() + ' ' + pct(r.chance); }).join(', ') +
				', then picks evenly among the items of that rarity (or the next lower one the table has).</div>' +
				table([{ text: 'Item' }, { text: 'Per drop', num: true }, { text: 'Chance', num: true }, { text: '' }], e.items.map(function (i) {
					return '<tr><td>' + item(i.item) + '</td><td class="num">' + pct(i.perDrop) + '</td><td class="num">' + pct(i.chance) + '</td><td>' +
						(i.missionDrop ? '<span class="badge bg-body-tertiary text-body border" title="Dropped only while a mission needs it">mission item</span>' : '') + '</td></tr>';
				}), 'cdc-odds') + '</div>';
		}).join(''); }) + '<div class="small text-body-secondary mt-2">Chances before any live event loot bonus. Mission items drop only for players whose mission needs them.</div>';
		return html;
	}

	// ---- views ----

	function objectView(lot) {
		api.get('/api/cdclient/objects/' + lot).then(function (d) {
			if (api404(d)) return showError(d.error);
			var sub = [esc(d.row.type || '')];
			if (d.item) sub.push(esc(d.item.itemTypeName));
			var html = header('<img src="/api/icon/' + lot + '" alt="" width="48" height="48" class="me-2 rounded" onerror="this.remove()">' + esc(d.name) +
				(d.item ? rarity(d.item.rarity) : '') + ' <span class="text-body-secondary fs-5">' + lot + '</span>', sub.filter(Boolean).join(' &middot; ') +
				(d.row._internalNotes ? ' &middot; ' + esc(d.row._internalNotes) : ''));
			if (d.description) html += '<p class="text-body-secondary">' + esc(d.description) + '</p>';

			var left = '', right = '';
			if (d.item) left += itemCard(d.item);
			if (d.destructible) left += destructibleCard(d.destructible);
			d.loot.forEach(function (l) {
				left += card('Drops (' + esc(l.table) + ', <a href="' + hashFor('loot_matrix', l.matrix) + '">loot matrix ' + esc(l.matrix) + '</a>)',
					lootOdds(l.odds, 'roll'));
			});
			if (d.vendor) left += vendorCard(d.vendor);
			if (d.skills.length) left += skillsCard(d.skills);
			if (d.inventory) left += card('Starts with (inventory component)', d.inventory.map(function (i) {
				return '<div>' + item(i.item) + ' &times; ' + num(i.count) + (i.equip ? ' <span class="badge bg-body-tertiary text-body border">equipped</span>' : '') + '</div>';
			}).join(''));
			if (d.missions) left += card('Missions here', d.missions.map(function (m) {
				return '<div>' + refLink(m.mission) + (m.offers ? ' <span class="badge text-bg-primary">offers</span>' : '') + (m.accepts ? ' <span class="badge text-bg-success">takes turn-ins</span>' : '') + '</div>';
			}).join(''));
			if (d.collectibleMission) left += card('Collectible', 'Needs ' + refLink(d.collectibleMission));

			right += d.model
				? card('Model', '<div id="modelViewer" class="cdc-model rounded overflow-hidden bg-body-tertiary d-flex align-items-center justify-content-center">' +
					'<button class="btn btn-sm btn-outline-primary" id="showModel">Show in 3D</button></div><div class="small text-body-secondary mt-1">' + esc(d.model) + '</div>')
				: '';
			right += sourcesCard(d.sources);
			if (d.rewardedBy.length) right += card('Mission rewards', d.rewardedBy.map(function (r) {
				return '<div>' + refLink(r.mission) + ' &times; ' + num(r.count) + (r.repeat ? ' <span class="badge bg-body-tertiary text-body border">on repeats</span>' : '') +
					(r.choice ? ' <span class="badge bg-body-tertiary text-body border">one of a choice</span>' : '') + '</div>';
			}).join(''));
			right += countedBy(d.countedBy);
			right += card('Components', d.components.length ? d.components.map(function (c) {
				var title = '<span class="fw-semibold">' + esc(c.typeName) + '</span> <span class="text-body-secondary">type ' + c.type +
					', id ' + c.componentId + (c.table ? ' in <a href="' + tableHash(c.table, { filters: [{ column: 'id', op: '=', value: String(c.componentId) }] }) + '">' + esc(c.table) + '</a>' : ', no table') + '</span>';
				// Nothing to unfold without rows (components with no table of their own)
				if (!c.rows.length) return '<div class="mb-2 ps-3">' + title + '</div>';
				return '<details class="mb-2"><summary>' + title + '</summary>' +
					(c.rows.length === 1 ? keyValues(c.rows[0], c.columns, c.table) : rowsTable(c.rows, c.table, c.columns)) + '</details>';
			}).join('') : none());
			right += card('Objects row', raw('All columns', keyValues(d.row, d.columns, 'Objects'))) + localized(d.localized);

			html += '<div class="row g-3"><div class="col-xl-6">' + left + '</div><div class="col-xl-6">' + right + '</div></div>';
			html += usedBy(d.usedBy, 'object', lot);
			view.innerHTML = html;
			var button = document.getElementById('showModel');
			if (button) button.addEventListener('click', function () { showModel(lot, d.name); });
		});
	}

	function itemCard(i) {
		var rows = [
			['Type', esc(i.itemTypeName) + ' <span class="text-body-secondary small">(' + esc(i.itemType) + ')</span>'],
			['Equips to', i.equipLocation ? '<span class="font-monospace">' + esc(i.equipLocation) + '</span>' : none('Not equipped')],
			['Stack size', num(i.stackSize)],
			['Base value', i.baseValue >= 0 ? num(i.baseValue) + ' coins <span class="small text-body-secondary">(a vendor charges it &times; its buy scalar)</span>' : none('No coin price')]
		];
		if (i.price.altCurrency) rows.push(['Also costs', item(i.price.altCurrency.item) + ' &times; ' + num(i.price.altCurrency.base) + ' <span class="small text-body-secondary">(&times; buy scalar)</span>']);
		if (i.price.costs.length) rows.push(['Crafting costs', items(i.price.costs, true)]);
		if (i.commendation) rows.push(['Commendation cost', item(i.commendation.item) + ' &times; ' + num(i.commendation.count)]);
		if (i.subItems.length) rows.push(['Equips with it', items(i.subItems)]);
		if (i.preconditions.length) rows.push(['Preconditions', i.preconditions.map(function (p) {
			return '<div><a href="' + tableHash('Preconditions', { filters: [{ column: 'id', op: '=', value: String(p.id) }] }) + '">' + esc(p.id) + '</a> ' + esc(p.reason) + '</div>';
		}).join('')]);
		var stats = i.stats || {};
		var bonuses = ['life', 'armor', 'imagination'].filter(function (k) { return stats[k]; }).map(function (k) { return '+' + esc(stats[k]) + ' ' + k; });
		if (bonuses.length) rows.push(['Stats', bonuses.join(', ')]);
		(stats.skills || []).forEach(function (s) { rows.push(['Skill', iconById(s.icon) + '<a href="' + hashFor('skill', s.id) + '">' + esc(s.description) + '</a>']); });
		if (i.set) rows.push(['Item set', iconById(i.set.icon) + esc(i.set.name || 'Set ' + i.set.id) + ' <span class="small text-body-secondary">rank ' + esc(i.set.rank) + '</span>' +
			i.set.bonuses.map(function (b) {
				var parts = ['life', 'armor', 'imagination'].filter(function (k) { return b.stats[k]; }).map(function (k) { return '+' + esc(b.stats[k]) + ' ' + k; })
					.concat((b.stats.skills || []).map(function (s) { return esc(s.description); }));
				return '<div class="small">' + esc(b.pieces) + ' pieces: ' + parts.join(', ') + '</div>';
			}).join('')]);
		return card('Item', '<table class="table table-sm mb-0"><tbody>' + rows.map(function (r) {
			return '<tr><th class="fw-normal text-body-secondary" style="width: 10rem">' + r[0] + '</th><td>' + r[1] + '</td></tr>';
		}).join('') + '</tbody></table>');
	}

	function destructibleCard(x) {
		return card('Health and smashing (destructible component)', '<div class="row text-center mb-2">' +
			stat('Life', num(x.life)) + stat('Armor', num(x.armor)) + stat('Imagination', num(x.imagination)) + stat('Level', num(x.level)) + '</div>' +
			'<div>Coins when smashed: ' + (x.coins ? num(x.coins.min) + '&ndash;' + num(x.coins.max) : none()) +
			' <span class="small text-body-secondary">(currency table at its level)</span></div>' +
			'<div class="small text-body-secondary">Faction ' + esc(x.faction) + (x.isSmashable ? ' &middot; smashable' : '') + '</div>');
	}

	function vendorCard(v) {
		var scalar = v.buyScalar ? 'prices are the base value &times; ' + num(v.buyScalar) : 'buy scalar 0: prices use the world\'s vendor_buy_multiplier';
		return card('Sells (vendor component, <a href="' + hashFor('loot_matrix', v.matrix) + '">loot matrix ' + esc(v.matrix) + '</a>)',
			'<div class="small text-body-secondary mb-2">' + scalar + '; buys back at &times; ' + num(v.sellScalar) +
			(v.refreshTimeSeconds > 0 ? '; restocks every ' + num(v.refreshTimeSeconds / 60) + ' min' : '') + '</div>' +
			v.stock.map(function (e) {
				return '<div class="mb-2"><div class="small"><a href="' + hashFor('loot_table', e.lootTable) + '">Loot table ' + esc(e.lootTable) + '</a>: ' +
					(e.all ? 'all of it' : (e.minToDrop === e.maxToDrop ? esc(e.minToDrop) : esc(e.minToDrop) + '&ndash;' + esc(e.maxToDrop)) + ' of its ' + e.items.length + ' items at random') + '</div>' +
					table([{ text: 'Item' }, { text: 'Price', num: true }, { text: 'In stock', num: true }], e.items.map(function (i) {
						var price = i.price.coins !== null ? num(i.price.coins) + ' coins' : '';
						if (i.price.altCurrency) price += (price ? ' + ' : '') + num(i.price.altCurrency.count !== null ? i.price.altCurrency.count : i.price.altCurrency.base) + ' ' + esc(i.price.altCurrency.item.name);
						i.price.costs.forEach(function (c) { price += (price ? ' + ' : '') + num(c.count) + ' ' + esc(c.item.name); });
						return '<tr><td>' + item(i.item) + '</td><td class="num">' + (price || none('-')) + '</td><td class="num">' + (i.sold ? pct(i.chance) : '<span class="text-body-secondary" title="No item component: vendors skip it">never</span>') + '</td></tr>';
					}), 'cdc-odds') + '</div>';
			}).join(''));
	}

	function skillsCard(skills) {
		return card('Skills', table([{ text: 'Skill' }, { text: 'Cost', num: true }, { text: 'Cooldown', num: true }, { text: 'castOnType <span class="fw-normal text-body-secondary">(raw)</span>', num: true }],
			skills.map(function (s) {
				return '<tr><td>' + iconById(s.skillIcon) + refLink(s.skill) + (s.description ? '<div class="small text-body-secondary">' + esc(s.description) + '</div>' : '') +
					(s.behaviorID ? '<div class="small"><a href="' + hashFor('behavior', s.behaviorID) + '">behavior ' + esc(s.behaviorID) + '</a></div>' : '') + '</td>' +
					'<td class="num">' + (s.imaginationcost ? num(s.imaginationcost) + ' imagination' : '') + '</td><td class="num">' + (s.cooldown ? num(s.cooldown) + ' s' : '') + '</td><td class="num">' + esc(s.castOnType) + '</td></tr>';
			})));
	}

	// Where an object comes from: drops, vendors, packages, activity rewards; with the chances
	function sourcesCard(sources) {
		if (!sources.length) return '';
		return card('Comes from', sources.map(function (s) {
			var columns = columnsOf(s.table);
			var rows = s.matrices.map(function (m) {
				// Objects with the component, or the rows themselves (with their links) for other tables
				var users = m.objects.slice(0, 5).map(function (o) { return '<div>' + item(o) + '</div>'; }).join('') +
					(m.objects.length > 5 ? '<div class="small text-body-secondary">and ' + (m.objects.length - 5) + ' more</div>' : '') +
					(m.rows || []).map(function (r) {
						return '<div class="small">' + columns.filter(function (c) { return r[c.name] !== null && r[c.name] !== '' && r[c.name] !== undefined; }).map(function (c) {
							return '<span class="text-body-secondary">' + esc(c.name) + '</span> ' + cell(r[c.name], c, r, s.table);
						}).join(' &middot; ') + '</div>';
					}).join('');
				return '<tr><td><a href="' + hashFor('loot_matrix', m.matrix) + '">' + esc(m.matrix) + '</a></td><td class="num">' +
					(m.chance !== undefined ? pct(m.chance) : '<span class="text-body-secondary" title="Only the first matrices\' odds are worked out">?</span>') +
					'</td><td class="num">' + (m.expected !== undefined ? num(+m.expected.toPrecision(3)) : '') + '</td><td>' + (users || none()) + '</td></tr>';
			});
			var head = [{ text: 'Loot matrix' }, { text: s.vendor ? 'In stock' : 'Chance', num: true }, { text: s.vendor ? '' : 'Average', num: true }, { text: 'Used by' }];
			return '<div class="mb-3"><div class="small text-body-secondary mb-1">Through ' + esc(s.table) + '.' + esc(s.column) + ' (' + rows.length + ')</div>' +
				table(head, rows.slice(0, 10), 'cdc-odds') +
				(rows.length > 10 ? later('All ' + rows.length, function () { return table(head, rows, 'cdc-odds'); }) : '') + '</div>';
		}).join('') + '<div class="small text-body-secondary">Chance per smash, opened package or activity reward, before live event bonuses; for vendors, the chance it is in stock.</div>');
	}

	function showModel(lot, name) {
		var container = document.getElementById('modelViewer');
		container.textContent = 'Loading…';
		import('/js/lddviewer.js').then(function (module) {
			disposeViewer();
			viewer = module.createViewer(container, {});
			return viewer.load([{ id: lot, name: name, lot: lot, position: [0, 0, 0], rotation: [0, 0, 0, 1], url: '/api/cdclient/objects/' + lot + '/lxfml' }], 1);
		}).catch(function (e) { container.textContent = 'The 3D view could not load: ' + e.message; });
	}

	// Prerequisites as the server reads them: each term joined to all the rest, so "a or b and c" is a or (b and c)
	function prerequisites(terms, text) {
		if (!terms.length || (terms.length === 1 && !terms[0].mission)) return none('None');
		function term(t) {
			if (!t.mission) return '<span class="badge text-bg-success">always met</span>';
			return '<span class="cdc-chip badge bg-body-tertiary text-body border">' + refLink(t.mission) + (t.stateName ? ' in state ' + esc(t.stateName) : ' complete') + '</span>';
		}
		function from(i) {
			var t = terms[i];
			if (i === terms.length - 1) return term(t);
			var rest = from(i + 1);
			// Brackets where the rest joins differently than this term does
			if (i + 1 < terms.length - 1 && terms[i + 1].or !== t.or) rest = '( ' + rest + ' )';
			return term(t) + ' <b>' + (t.or ? 'or' : 'and') + '</b> ' + rest;
		}
		return '<div class="cdc-expr">' + from(0) + '</div><div class="small text-body-secondary mt-1">As the server reads <span class="font-monospace">' + esc(text) + '</span>: ' +
			'each mission is joined to everything after it, and brackets don\'t group.</div>';
	}

	function taskCard(tasks) {
		if (!tasks.length) return card('Tasks', none());
		return card('Tasks', tasks.map(function (t) {
			var parts = [];
			if (t.targets.length) parts.push('<div><span class="small text-body-secondary">Targets:</span> ' + t.targets.map(item).join(', ') + '</div>');
			if (t.targetText) parts.push('<div><span class="small text-body-secondary">Target group:</span> <span class="font-monospace">' + esc(t.targetText) + '</span></div>');
			if (t.parameters.length) parts.push('<div><span class="small text-body-secondary">Parameters:</span> ' + t.parameters.map(item).join(', ') + '</div>');
			if (t.racingParameter) parts.push('<div><span class="small text-body-secondary">Racing task:</span> ' + esc(t.racingParameter) + '</div>');
			return '<div class="mb-3">' + iconById(t.iconID) + '<span class="badge text-bg-secondary">' + esc(t.taskTypeName) + '</span> ' +
				'<span class="fw-semibold">&times; ' + num(t.targetValue) + '</span> ' + esc(t.description || '') +
				(t.progressed ? '' : ' <span class="badge text-bg-warning" title="MissionTask::Progress has no case for this type">the server never progresses this type</span>') +
				parts.join('') + raw('MissionTasks row (uid ' + esc(t.uid) + ')', keyValues(t.row, columnsOf('MissionTasks'), 'MissionTasks')) + '</div>';
		}).join(''));
	}

	function rewardsCard(r, row) {
		var choice = row.isChoiceReward ? ' <span class="small text-body-secondary">(the player picks one)</span>' : '';
		var first = '<h6 class="small text-body-secondary">First completion' + choice + '</h6>' + (r.items.length ? items(r.items, true) : '');
		var extras = [];
		if (row.reward_currency > 0) extras.push(num(row.reward_currency) + ' coins');
		if (row.LegoScore > 0) extras.push(num(row.LegoScore) + ' U-score');
		if (row.reward_reputation > 0) extras.push(num(row.reward_reputation) + ' reputation');
		first += extras.length ? '<div>' + extras.join(', ') + '</div>' : '';
		if (r.emotes.length) first += '<div><span class="small text-body-secondary">Emotes:</span> ' + r.emotes.map(refLink).join(', ') + '</div>';
		if (r.stats.length) first += r.stats.map(function (s) { return '<div><span class="font-monospace small">' + esc(s.column) + '</span> +' + num(s.value) + '</div>'; }).join('');
		if (!r.items.length && !extras.length && !r.emotes.length && !r.stats.length) first += none();
		var html = first;
		if (row.repeatable) {
			var again = [];
			if (row.reward_currency_repeatable > 0) again.push(num(row.reward_currency_repeatable) + ' coins');
			if (row.LegoScore > 0) again.push(num(row.LegoScore) + ' U-score');
			if (row.reward_reputation > 0) again.push(num(row.reward_reputation) + ' reputation');
			html += '<h6 class="small text-body-secondary mt-3">Each repeat' + choice + '</h6>' + items(r.repeatItems, true) + (again.length ? '<div>' + again.join(', ') + '</div>' : '');
		}
		return card('Rewards', html + '<div class="small text-body-secondary mt-2">At the level cap U-score is paid as coins.</div>');
	}

	function missionView(id) {
		api.get('/api/cdclient/missions/' + id).then(function (d) {
			if (api404(d)) return showError(d.error);
			var row = d.row;
			var badges = [row.isMission ? 'Mission' : 'Achievement'];
			if (row.repeatable) badges.push('Repeatable' + (row.cooldownTime > 0 ? ' every ' + num(row.cooldownTime) + ' min' : ''));
			if (row.time_limit > 0) badges.push('time_limit ' + num(row.time_limit));
			var html = header(iconById(row.missionIconID, 40) + esc(d.name) + ' <span class="text-body-secondary fs-5">' + id + '</span>',
				esc([row.defined_type, row.defined_subtype].filter(Boolean).join(' / ')) + ' ' + badges.map(function (b) { return '<span class="badge bg-body-tertiary text-body border">' + b + '</span>'; }).join(' '));
			html += '<div class="row g-3"><div class="col-xl-6">';
			html += taskCard(d.tasks) + rewardsCard(d.rewards, row);
			html += card('Prerequisites', prerequisites(d.prerequisites, row.prereqMissionID || ''));
			html += card('Unlocks', chips(d.unlocks)) + countedBy(d.countedBy);
			html += '</div><div class="col-xl-6">';
			html += card('Offered by', chips(d.offeredBy)) + card('Turned in to', chips(d.acceptedBy));
			html += localized(d.text) + localized(d.localized) + card('Missions row', raw('All columns', keyValues(row, d.columns, 'Missions'))) + '</div></div>';
			html += usedBy(d.usedBy, 'mission', id);
			view.innerHTML = html;
		});
	}

	// A behavior tree, nested; each behavior with its template (from the server's enum) and parameters
	function behaviorTree(tree) {
		var shown = {};
		function node(behaviorId, label) {
			var n = tree.nodes[String(behaviorId)];
			var title = (label ? '<span class="text-body-secondary">' + esc(label) + ':</span> ' : '') +
				'<a href="' + hashFor('behavior', behaviorId) + '">' + esc(behaviorId) + '</a> ';
			if (!n) return '<div class="ms-3">' + title + '<span class="text-danger">not in BehaviorTemplate' + (tree.truncated ? ' (or past the depth shown)' : '') + '</span></div>';
			title += '<span class="fw-semibold">' + esc(n.templateName) + '</span> <span class="small text-body-secondary">template ' + esc(n.templateID) + '</span>' +
				(n.effectID ? ' <span class="small text-body-secondary">effect ' + esc(n.effectID) + (n.effectHandle ? ' ' + esc(n.effectHandle) : '') + '</span>' : '');
			if (shown[behaviorId]) return '<div class="ms-3">' + title + ' <span class="small text-body-secondary">(shown above)</span></div>';
			shown[behaviorId] = true;
			var values = n.parameters.filter(function (p) { return p.child === null; });
			var children = n.parameters.filter(function (p) { return p.child !== null; });
			return '<details open><summary>' + title + '</summary>' +
				(values.length ? '<table class="params"><tbody>' + values.map(function (p) {
					return '<tr><td class="text-body-secondary">' + esc(p.name) + '</td><td>' + num(p.value) + '</td></tr>';
				}).join('') + '</tbody></table>' : '') +
				children.map(function (p) { return node(p.child, p.name); }).join('') + '</details>';
		}
		return '<div class="cdc-tree">' + node(tree.root, '') + '</div>' + (tree.truncated ? '<div class="small text-warning">Cut short: too big to show whole.</div>' : '');
	}

	function skillView(id) {
		api.get('/api/cdclient/skills/' + id).then(function (d) {
			if (api404(d)) return showError(d.error);
			var row = d.row;
			var html = header(iconById(row.skillIcon, 40) + esc(d.name) + ' <span class="text-body-secondary fs-5">' + id + '</span>', esc(d.description || ''));
			html += '<div class="row text-center mb-3">' + stat('Imagination cost', num(row.imaginationcost || 0)) + stat('Cooldown', num(row.cooldown || 0) + ' s') +
				stat('Cooldown group', num(row.cooldowngroup || 0)) + '</div>';
			html += '<div class="row g-3"><div class="col-xl-7">' + card('Behavior tree' + (row.behaviorID ? ' <a href="' + hashFor('behavior', row.behaviorID) + '" class="small fw-normal">' + esc(row.behaviorID) + '</a>' : ''),
				d.tree ? behaviorTree(d.tree) : none('No behavior')) + '</div><div class="col-xl-5">';
			html += card('On objects', d.objects.length ? table([{ text: 'Object' }, { text: 'castOnType <span class="fw-normal text-body-secondary">(raw)</span>', num: true }], d.objects.map(function (o) {
				return '<tr><td>' + item(o.object) + '</td><td class="num">' + esc(o.castOnType) + '</td></tr>';
			})) : none());
			html += countedBy(d.countedBy) + localized(d.localized) + card('SkillBehavior row', raw('All columns', keyValues(row, d.columns, 'SkillBehavior'))) + '</div></div>';
			view.innerHTML = html + usedBy(d.usedBy, 'skill', id);
		});
	}

	function behaviorView(id) {
		api.get('/api/cdclient/behaviors/' + id).then(function (d) {
			if (api404(d)) return showError(d.error);
			var count = Object.keys(d.tree.nodes).length;
			view.innerHTML = header('Behavior ' + id, nf.format(count) + (count === 1 ? ' behavior' : ' behaviors') + ' in this tree') +
				card('Tree', behaviorTree(d.tree)) + usedBy(d.usedBy, 'behavior', id);
		});
	}

	function activityView(id) {
		api.get('/api/cdclient/activities/' + id).then(function (d) {
			if (api404(d)) return showError(d.error);
			var row = d.row;
			var html = header(esc(d.name) + ' <span class="text-body-secondary fs-5">' + id + '</span>', d.zone ? 'Played in ' + refLink(d.zone) : '');
			html += '<p>Costs to play: ' + (d.cost ? item(d.cost.item) + ' &times; ' + num(d.cost.count) : none('nothing')) + '</p>';
			var rewards = d.rewards.map(function (r, i) {
				return '<div class="mb-3"><div><span class="fw-semibold">Rating ' + esc(r.rating) + '</span>' + (i === 0 ? ' <span class="badge bg-body-tertiary text-body border" title="Activity instances give the first reward row">first row</span>' : '') +
					(r.description ? ' <span class="text-body-secondary">' + esc(r.description) + '</span>' : '') + '</div>' +
					'<div class="small">Coins: ' + (r.coins ? num(r.coins.min) + '&ndash;' + num(r.coins.max) : none()) + (r.matrix > 0 ? ' &middot; <a href="' + hashFor('loot_matrix', r.matrix) + '">loot matrix ' + esc(r.matrix) + '</a>' : '') + '</div>' +
					(r.odds ? raw('Drops', lootOdds(r.odds, 'reward')) : '') + '</div>';
			}).join('');
			html += '<div class="row g-3"><div class="col-xl-7">' + card('Rewards', (rewards || none()) + '<div class="small text-body-secondary">Scripts that rate a result give the row with the highest rating at or below it; ' +
				'activity instances give the first row. Coins come from the currency table at level 1.</div>') + '</div><div class="col-xl-5">';
			html += countedBy(d.countedBy) + localized(d.localized) + card('Activities row', raw('All columns', keyValues(row, d.columns, 'Activities'))) + '</div></div>';
			view.innerHTML = html + usedBy(d.usedBy, 'activity', id);
		});
	}

	function zoneView(id) {
		api.get('/api/cdclient/zones/' + id).then(function (d) {
			if (api404(d)) return showError(d.error);
			var c = d.caps;
			var override = function (value) { return value !== null ? ' <span class="badge text-bg-primary" title="Set on the Instances page">override</span>' : ''; };
			var html = header(esc(d.name) + ' <span class="text-body-secondary fs-5">' + id + '</span>', esc(d.row.mapFolder || '') + (d.row.zoneName ? ' &middot; ' + esc(d.row.zoneName) : ''));
			html += '<div class="row g-3"><div class="col-xl-6">';
			html += card('Players per instance', '<div class="row text-center mb-2">' + stat('Soft cap' + override(c.softOverride), num(c.soft)) + stat('Hard cap' + override(c.hardOverride), num(c.hard)) + '</div>' +
				'<div class="small text-body-secondary">The master sends players to a running instance while it has fewer than the soft cap (friends joining them up to the hard cap), ' +
				'otherwise starts a new one. The ZoneTable says ' + num(c.clientSoft) + ' and ' + num(c.clientHard) + '.' +
				(c.overridesShown ? '' : ' Overrides set on the Instances page aren\'t shown without access to it.') + ' <a href="/instances">Instances</a></div>');
			html += card('Zone control object', d.control ? item(d.control) : none('None set: the server spawns its default zone control object'));
			html += '</div><div class="col-xl-6">' + localized(d.localized) + card('ZoneTable row', raw('All columns', keyValues(d.row, d.columns, 'ZoneTable'))) + '</div></div>';
			view.innerHTML = html + usedBy(d.usedBy, 'zone', id);
		});
	}

	function lootMatrixView(id) {
		api.get('/api/cdclient/loot_matrix/' + id).then(function (d) {
			if (api404(d)) return showError(d.error);
			view.innerHTML = header('Loot matrix ' + id) + card('Drops', lootOdds(d.odds, 'roll')) + usedBy(d.usedBy, 'loot_matrix', id);
		});
	}

	function lootTableView(id) {
		api.get('/api/cdclient/loot_table/' + id).then(function (d) {
			if (api404(d)) return showError(d.error);
			view.innerHTML = header('Loot table ' + id, nf.format(d.items.length) + ' items, highest rarity first as the server sorts them') +
				card('Items', table([{ text: 'Item' }, { text: 'Rarity', num: true }, { text: '' }], d.items.map(function (i) {
					return '<tr><td>' + item(i.item) + '</td><td class="num">' + (rarity(i.rarity) || none('0')) + '</td><td>' +
						(i.missionDrop ? '<span class="badge bg-body-tertiary text-body border" title="Dropped only while a mission needs it">mission item</span>' : '') + '</td></tr>';
				}), 'cdc-odds') + '<div class="small text-body-secondary mt-2">The chance of each depends on the loot matrix and rarity table it is rolled with: open one of the matrices below.</div>') +
				usedBy(d.usedBy, 'loot_table', id);
		});
	}

	// ---- routing ----

	function route() {
		disposeViewer();
		lazy = {};
		var hash = window.location.hash.replace(/^#\/?/, '');
		var queryAt = hash.indexOf('?');
		var path = (queryAt === -1 ? hash : hash.slice(0, queryAt)).split('/').map(decodeURIComponent);
		var params = {};
		if (queryAt !== -1) hash.slice(queryAt + 1).split('&').forEach(function (p) {
			var eq = p.indexOf('=');
			if (eq > 0) params[p.slice(0, eq)] = decodeURIComponent(p.slice(eq + 1));
		});
		view.innerHTML = '<div class="text-body-secondary">Loading&hellip;</div>';
		window.scrollTo(0, 0);
		var id = parseInt(path[1], 10);
		switch (path[0]) {
			case '': return home();
			case 'search': return search(path.slice(1).join('/'));
			case 'table': return tableView(path[1], params);
			case 'object': return objectView(id);
			case 'mission': return missionView(id);
			case 'skill': return skillView(id);
			case 'behavior': return behaviorView(id);
			case 'activity': return activityView(id);
			case 'zone': return zoneView(id);
			case 'loot_matrix': return lootMatrixView(id);
			case 'loot_table': return lootTableView(id);
			default: return showError('Nothing here.');
		}
	}

	document.getElementById('searchForm').addEventListener('submit', function (e) {
		e.preventDefault();
		var text = document.getElementById('searchInput').value.trim();
		if (text) window.location.hash = '#/search/' + encodeURIComponent(text);
	});
	window.addEventListener('hashchange', route);

	api.get('/api/cdclient/schema').then(function (d) {
		if (api404(d)) return showError(d.error);
		schema = d;
		route();
	});
})();
