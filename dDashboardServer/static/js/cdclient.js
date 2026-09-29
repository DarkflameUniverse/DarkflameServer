/**
 * The CDClient Browser page: a raw table viewer. Everything is addressed by the hash, so views can be linked and the
 * back button works:
 *   #/                       the list of tables
 *   #/table/<Name>?...       a table's rows (start, order, dir, q, filters as JSON [{column, op, value}])
 *   #/<link>/<id>            shorthand for the target table filtered to that ID (#/object/1727 is Objects id = 1727)
 * Which columns link where comes from /api/cdclient/schema; a linked value opens the target table filtered to it.
 */
(function () {
	'use strict';

	var PAGE = 50;
	var OPS = ['=', '!=', '<', '<=', '>', '>=', 'contains', 'starts', 'null', 'notnull'];
	var nf = new Intl.NumberFormat();
	var view = document.getElementById('view');
	var schema = null;

	function api404(d) { return d && d.success === false; }

	// ---- links ----

	function tableHash(table, params) {
		var query = [];
		Object.keys(params || {}).forEach(function (k) {
			var v = params[k];
			if (v === undefined || v === null || v === '' || (Array.isArray(v) && !v.length)) return;
			query.push(k + '=' + encodeURIComponent(Array.isArray(v) ? JSON.stringify(v) : v));
		});
		return '#/table/' + encodeURIComponent(table) + (query.length ? '?' + query.join('&') : '');
	}

	// The rows a link points at: its target table filtered to the ID
	function hashFor(link, id) {
		var target = schema && schema.links[link];
		if (!target) return null;
		return tableHash(target.table, { filters: [{ column: target.column, op: '=', value: String(id) }] });
	}

	function componentTable(type) {
		return schema.componentTypes.filter(function (c) { return c.type === type; })[0] || null;
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

	function card(title, body) {
		return '<div class="card mb-3"><div class="card-header"><h5 class="mb-0 fs-6">' + title + '</h5></div><div class="card-body">' + body + '</div></div>';
	}

	function header(title, subtitle) {
		return '<div class="mb-3"><h3 class="mb-0">' + title + '</h3>' + (subtitle ? '<div class="text-body-secondary small">' + subtitle + '</div>' : '') + '</div>';
	}

	function showError(message) {
		view.innerHTML = '<div class="alert alert-warning">' + esc(message) + '</div>';
	}

	// ---- views ----

	function home() {
		var tables = schema.tables;
		view.innerHTML = card('Tables <span class="badge text-bg-secondary">' + tables.length + '</span>',
			'<input class="form-control form-control-sm mb-2" id="tableFilter" placeholder="Filter tables" aria-label="Filter tables">' +
			'<div class="cdc-tables" id="tableLinks">' + tables.map(function (t) {
				return '<a href="' + tableHash(t.name) + '" data-name="' + esc(t.name.toLowerCase()) + '">' + esc(t.name) + '</a>';
			}).join('') + '</div>');
		var filter = document.getElementById('tableFilter');
		filter.addEventListener('input', function () {
			var text = filter.value.trim().toLowerCase();
			document.querySelectorAll('#tableLinks a').forEach(function (a) { a.classList.toggle('d-none', text && a.dataset.name.indexOf(text) === -1); });
		});
		// Enter opens the only table left
		filter.addEventListener('keydown', function (e) {
			if (e.key !== 'Enter') return;
			var shown = document.querySelectorAll('#tableLinks a:not(.d-none)');
			if (shown.length === 1) window.location.hash = shown[0].getAttribute('href');
		});
		filter.focus();
	}

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
			view.innerHTML = header('<a href="#/" class="text-decoration-none">Tables</a> / ' + esc(name), nf.format(d.total) + ' matching rows') +
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

	// ---- routing ----

	function route() {
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
		if (path[0] === '') return home();
		if (path[0] === 'table') return tableView(path[1], params);
		// #/object/1727 and the like: the rows it names, without adding a history entry
		var id = parseInt(path[1], 10);
		var target = !isNaN(id) && hashFor(path[0], id);
		if (target) return window.location.replace(target);
		showError('Nothing here.');
	}

	window.addEventListener('hashchange', route);

	api.get('/api/cdclient/schema').then(function (d) {
		if (api404(d)) return showError(d.error);
		schema = d;
		route();
	});
})();
