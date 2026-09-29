/**
 * The UGC Server page: counts, the UGC server's live status, and what it made: player models, and cars and rockets as
 * assemblies (one per combination of modules, however many builds use it), paged on the server with a search box
 * (field prefixes), filters and sorting, all kept in the address so Back and links work. An item opens with its
 * generated mesh and LXFML (models), its modules and the builds that use it (assemblies), and the icon editor
 * (ugc-icon-editor.js). Everything from the UGC server comes through the dashboard, so the browser never has to reach
 * the UGC server itself.
 */
(function () {
	'use strict';

	var STATES = { pending: ['Waiting', 'secondary'], done: ['Made', 'success'], failed: ['Failed', 'danger'], empty: ['Empty', 'light'] };
	var SORTS = {
		model: [['newest', 'Newest'], ['oldest', 'Oldest'], ['bricks', 'Most bricks'], ['triangles', 'Most triangles'], ['made', 'Recently made'], ['slowest', 'Slowest to make'], ['cpu', 'Most CPU'], ['memory', 'Most RAM'], ['savings', 'Most triangles saved'], ['owner', 'Owner'], ['name', 'File name']],
		modular: [['newest', 'Newest'], ['oldest', 'Oldest'], ['references', 'Most builds'], ['name', 'Name']]
	};
	// The list's state, as in the address: ?kind=&q=&state=&type=&sort=&page= (from 1)&view=, and the open item (&item=, &build=)
	var list = { kind: 'model', q: '', state: '', type: '', sort: 'newest', page: 0, view: 'gallery' };
	var canManage = false, publicUrl = '', items = [], total = 0, kinds = [];
	var nifViewer = null, lxfmlViewer = null, current = null;

	function $(id) { return document.getElementById(id); }
	function fileUrl(itemKind, id, name) { return '/api/ugc/files/' + itemKind + '/' + encodeURIComponent(id) + '/' + name; }
	function mb(bytes) { return (bytes / 1048576).toFixed(0) + ' MB'; }
	function pageSize() { return +Prefs.get('ugc.pageSize.' + list.view, list.view === 'gallery' ? 48 : 50) || 48; }

	// ---- the address ----

	function readUrl() {
		var p = new URLSearchParams(location.search);
		list.kind = p.get('kind') === 'modular' ? 'modular' : 'model';
		list.q = p.get('q') || '';
		list.state = p.get('state') || '';
		list.type = p.get('type') || '';
		list.sort = p.get('sort') || 'newest';
		list.page = Math.max(0, (parseInt(p.get('page'), 10) || 1) - 1);
		var v = p.get('view');
		if (v === 'list' || v === 'gallery') list.view = v;
		else try { list.view = localStorage.getItem('ugcView') === 'list' ? 'list' : 'gallery'; } catch (e) { /* storage blocked */ }
		var item = p.get('item');
		return item && /^[\d-]{1,200}$/.test(item) ? { id: item, kind: list.kind, build: p.get('build') || '' } : null;
	}
	function writeUrl(push, item) {
		var p = new URLSearchParams();
		p.set('kind', list.kind);
		if (list.q) p.set('q', list.q);
		if (list.state) p.set('state', list.state);
		if (list.type && list.kind === 'modular') p.set('type', list.type);
		if (list.sort !== 'newest') p.set('sort', list.sort);
		if (list.page) p.set('page', list.page + 1);
		p.set('view', list.view);
		if (item) {
			p.set('item', item.id);
			if (item.build) p.set('build', item.build);
		}
		var url = location.pathname + '?' + p.toString();
		if (url === location.pathname + location.search) return;
		if (push) history.pushState(null, '', url);
		else history.replaceState(null, '', url);
	}
	function showControls() {
		$('kindButtons').querySelectorAll('[data-kind]').forEach(function (b) { b.classList.toggle('active', b.dataset.kind === list.kind); });
		$('viewButtons').querySelectorAll('[data-view]').forEach(function (b) { b.classList.toggle('active', b.dataset.view === list.view); });
		if (document.activeElement !== $('search')) $('search').value = list.q;
		$('stateFilter').value = list.state;
		$('typeFilter').classList.toggle('d-none', list.kind !== 'modular');
		$('typeFilter').value = list.type;
		$('sortSelect').innerHTML = SORTS[list.kind].map(function (o) { return '<option value="' + o[0] + '">' + esc(o[1]) + '</option>'; }).join('');
		$('sortSelect').value = SORTS[list.kind].some(function (o) { return o[0] === list.sort; }) ? list.sort : 'newest';
		$('sortSelect').classList.toggle('d-none', list.view !== 'gallery'); // the List view sorts by its column headings
		$('pageSize').value = String(pageSize());
	}

	// ---- the list ----

	function countCard(title, c) {
		return '<div class="col-md-6"><div class="card"><div class="card-body py-2"><div class="small text-body-secondary">' + esc(title) + '</div>' +
			fmt.badge(c.done + ' made', 'success') + ' ' + fmt.badge(c.pending + ' waiting', 'secondary') + ' ' + fmt.badge(c.failed + ' failed', c.failed ? 'danger' : 'secondary') +
			(c.empty ? ' ' + fmt.badge(c.empty + ' empty', 'light') : '') + '</div></div></div>';
	}
	function owner(i) {
		return i.characterName ? '<a href="/characters/' + esc(i.characterId) + '">' + esc(i.characterName) + '</a>' : '<span class="text-body-secondary">' + esc(i.characterId) + '</span>';
	}
	function badge(state) { var s = STATES[state] || [state, 'secondary']; return fmt.badge(s[0], s[1]); }
	// The id whose files show an item's icon: a model's own, an assembly's made build's (the UGC server finds its combination)
	function iconId(i) { return list.kind === 'modular' ? i.iconBuild : i.id; }
	function iconImg(i, size) {
		if (i.state !== 'done') return '<span class="ugc-noicon text-body-secondary small border rounded" style="width:' + size + 'px;height:' + size + 'px">' + esc((STATES[i.state] || [i.state])[0]) + '</span>';
		return '<img src="' + esc(fileUrl(list.kind, iconId(i), 'icon.png')) + '" width="' + size + '" height="' + size + '" loading="lazy" alt="" class="ugc-checker rounded" onerror="this.style.visibility=\'hidden\'">';
	}
	function moduleNames(i) { return (i.moduleList || []).map(function (m) { return m.name || m.lot; }).join(', '); }
	// A duration in the largest units that fit: "850 ms", "12.4 s", "2 min 5 s", "1 h 4 min", "2 d 3 h"
	function duration(ms) {
		ms = Math.round(ms);
		if (ms < 1000) return ms + ' ms';
		if (ms < 60000) return (ms / 1000).toFixed(ms < 10000 ? 2 : 1) + ' s';
		var s = Math.round(ms / 1000), m = Math.floor(s / 60), h = Math.floor(m / 60), d = Math.floor(h / 24);
		if (d) return d + ' d' + (h % 24 ? ' ' + (h % 24) + ' h' : '');
		if (h) return h + ' h' + (m % 60 ? ' ' + (m % 60) + ' min' : '');
		return m + ' min' + (s % 60 ? ' ' + (s % 60) + ' s' : '');
	}
	function megabytes(kb) { return kb >= 1048576 ? (kb / 1048576).toFixed(1) + ' GB' : kb >= 1024 ? Math.round(kb / 1024) + ' MB' : kb + ' KB'; }
	// What the last make cost: its time, the worker's CPU time and the memory the UGC server estimated for it
	function costText(i) {
		if (!i.processMs) return '';
		return 'took ' + duration(i.processMs) + (i.processCpuMs ? ', CPU ' + duration(i.processCpuMs) : '') + (i.processMemoryKb ? ', ~' + megabytes(i.processMemoryKb) + ' RAM (est.)' : '');
	}
	// When it was made and what that cost (the cost is only known for makes since it was recorded)
	function madeText(i) {
		if (!i.processedAt) return '';
		return fmt.unix(i.processedAt) + (costText(i) ? ' \u00b7 ' + costText(i) : '');
	}
	// A model's file as the player named it: its name with the upload's extension (".lxfml"), else the upload's name
	function fileName(i) {
		var file = i.detail || '';
		if (!i.modelName) return file;
		var dot = file.lastIndexOf('.');
		return i.modelName + (dot > 0 ? file.slice(dot) : '');
	}
	function waitBadge(i) {
		return i.state === 'pending' && i.processAfter > Date.now() / 1000 ? ' <span class="small text-body-secondary" title="Waits for the owner to stop saving">after ' + esc(fmt.unix(i.processAfter)) + '</span>' : '';
	}
	function tile(i) {
		var title = list.kind === 'modular' ? esc(i.kindLabel || 'Build type ' + i.buildType) : (i.characterName ? esc(i.characterName) : '<span class="text-body-secondary">' + esc(i.characterId) + '</span>');
		var sub = list.kind === 'modular' ? esc(moduleNames(i)) : esc(i.id);
		var extra = list.kind === 'modular' ? ' ' + fmt.badge(i.uses + ' build' + (i.uses === 1 ? '' : 's'), 'info') : waitBadge(i);
		var tip = i.error || (list.kind === 'modular' ? moduleNames(i) : i.id) + (madeText(i) ? '\nMade ' + madeText(i) : '');
		return '<div class="card ugc-tile p-2" tabindex="0" role="button" data-preview="' + esc(i.id) + '" title="' + esc(tip) + '">' + iconImg(i, 128) +
			'<div class="small text-truncate mt-1">' + title + '</div><div class="small text-body-secondary text-truncate">' + sub + '</div><div>' + badge(i.state) + extra + '</div></div>';
	}
	function actions(i, remakeId) {
		return '<button type="button" class="btn btn-sm btn-outline-secondary me-1" data-preview="' + esc(i.id) + '">View</button>' +
			(canManage ? '<button type="button" class="btn btn-sm btn-outline-warning" data-remake="' + esc(remakeId) + '">Make again</button>' : '');
	}
	function errorText(i) { return i.error ? '<div class="small text-danger">' + esc(i.error) + '</div>' : ''; }

	// The List view's columns. A column with a sort key sorts on the server by that key; desc marks the key's own
	// direction (e.g. "bricks" is the most first), so clicking the other way asks for the reverse.
	function column(title, render, sort, desc) {
		return { data: null, title: title, orderable: !!sort, sortKey: sort, desc: !!desc, orderSequence: desc ? ['desc', 'asc'] : ['asc', 'desc'],
			render: function (x, type, i) { return render(i); } };
	}
	var COLUMNS = {
		model: [
			column('Icon', function (i) { return iconImg(i, 48); }),
			column('ID', function (i) { return '<span class="small">' + esc(i.id) + '</span>'; }, 'newest', true),
			column('Owner', owner, 'owner'),
			column('State', function (i) {
				return badge(i.state) + (i.attempts ? ' <span class="small text-body-secondary">' + esc(i.attempts) + ' attempt' + (i.attempts === 1 ? '' : 's') + '</span>' : '') + waitBadge(i);
			}),
			column('Made', function (i) { return '<span class="small">' + (i.processedAt ? esc(fmt.unix(i.processedAt)) : '') + '</span>'; }, 'made', true),
			column('Took', function (i) { return '<span class="small">' + (i.processMs ? esc(duration(i.processMs)) : '') + '</span>'; }, 'slowest', true),
			column('CPU', function (i) { return '<span class="small">' + (i.processCpuMs ? esc(duration(i.processCpuMs)) : '') + '</span>'; }, 'cpu', true),
			column('RAM (est.)', function (i) { return '<span class="small">' + (i.processMemoryKb ? '~' + esc(megabytes(i.processMemoryKb)) : '') + '</span>'; }, 'memory', true),
			column('Saved', function (i) {
				if (!i.trianglesBefore) return '';
				var removed = i.trianglesBefore - i.triangles, share = removed / i.trianglesBefore * 100;
				return '<span class="small" title="' + esc(i.trianglesBefore.toLocaleString()) + ' triangles before, ' + esc(i.triangles.toLocaleString()) + ' after">' +
					esc(share.toFixed(share < 10 ? 1 : 0)) + '%<br><span class="text-body-secondary">' + esc(removed.toLocaleString()) + ' tris</span></span>';
			}, 'savings', true),
			column('Size', function (i) { return '<span class="small">' + (i.bricks ? esc(i.bricks) + ' bricks<br>' + esc(i.triangles.toLocaleString()) + ' triangles' : '') + '</span>'; }, 'bricks', true),
			column('File', function (i) { return '<span class="small" title="' + esc(i.detail || '') + '">' + esc(fileName(i)) + '</span>' + errorText(i); }, 'name'),
			column('', function (i) { return '<div class="text-end text-nowrap">' + actions(i, i.id) + '</div>'; })
		],
		modular: [
			column('Icon', function (i) { return iconImg(i, 48); }),
			column('Newest build', function (i) { return '<span class="small">' + esc(i.newestBuild) + '</span>'; }, 'newest', true),
			column('Type', function (i) { return esc(i.kindLabel || i.buildType); }),
			column('Modules', function (i) { return '<span class="small">' + esc(moduleNames(i)) + '</span><div><code>' + esc(i.key) + '</code></div>' + errorText(i); }, 'name'),
			column('State', function (i) { return badge(i.state); }),
			column('Builds', function (i) { return esc(i.uses); }, 'references', true),
			column('Owners', function (i) { return esc(i.owners); }),
			column('', function (i) { return '<div class="text-end text-nowrap">' + actions(i, i.iconBuild) + '</div>'; })
		]
	};
	// The sort a DataTables order asks for, as /api/ugc's sort= and reverse=
	function orderQuery(columns, order) {
		var o = order && order[0], c = o && columns[o.column];
		if (!c || !c.sortKey) return 'sort=newest';
		return 'sort=' + c.sortKey + ((o.dir === 'desc') !== c.desc ? '&reverse=1' : '');
	}

	// Numbered pages with first and last, around the current one
	function pager(el, page, pages, onPage) {
		if (pages <= 1) { el.innerHTML = ''; return; }
		var shown = [];
		for (var p = Math.max(0, page - 2); p <= Math.min(pages - 1, page + 2); p++) shown.push(p);
		var li = function (p, label, disabled, active) {
			return '<li class="page-item' + (disabled ? ' disabled' : '') + (active ? ' active' : '') + '"><a class="page-link" href="#" data-page="' + p + '"' +
				(active ? ' aria-current="page"' : '') + '>' + label + '</a></li>';
		};
		var html = li(0, '&laquo;', page === 0) + li(Math.max(0, page - 1), '&lsaquo;', page === 0);
		if (shown[0] > 0) html += li(0, '1') + (shown[0] > 1 ? '<li class="page-item disabled"><span class="page-link">&hellip;</span></li>' : '');
		shown.forEach(function (p) { html += li(p, p + 1, false, p === page); });
		if (shown[shown.length - 1] < pages - 1) html += (shown[shown.length - 1] < pages - 2 ? '<li class="page-item disabled"><span class="page-link">&hellip;</span></li>' : '') + li(pages - 1, pages);
		html += li(Math.min(pages - 1, page + 1), '&rsaquo;', page >= pages - 1) + li(pages - 1, '&raquo;', page >= pages - 1);
		el.innerHTML = html;
		el.onclick = function (e) {
			var a = e.target.closest('[data-page]');
			if (!a || a.parentElement.classList.contains('disabled')) return;
			e.preventDefault();
			onPage(+a.dataset.page);
		};
	}

	// What every /api/ugc answer carries besides the items
	function applyMeta(d) {
		canManage = d.canManage;
		publicUrl = (d.ugcPublicUrl || '').replace(/\/+$/, '');
		if (d.kinds && !kinds.length) {
			kinds = d.kinds;
			$('typeFilter').innerHTML = '<option value="">Every type</option>' + kinds.filter(function (k) { return k.buildType !== undefined; })
				.map(function (k) { return '<option value="' + esc(k.kind) + '">' + esc(k.label) + '</option>'; }).join('');
			$('typeFilter').value = list.type;
		}
		$('manageButtons').classList.toggle('d-none', !canManage);
		$('cacheCard').classList.toggle('d-none', !canManage);
		$('counts').innerHTML = countCard('Models', d.counts.model) + countCard('Cars and rockets (builds)', d.counts.modular);
	}
	function filterQuery() {
		return 'kind=' + list.kind + '&q=' + encodeURIComponent(list.q) + '&state=' + list.state + '&type=' + encodeURIComponent(list.kind === 'modular' ? list.type : '');
	}

	// The List view: a server-side DataTable per kind, made the first time it shows. Search and filters stay the
	// page's own (above); DataTables does the paging, and sorting by the column headings.
	var tables = {}, tableLoaded = null;
	function listTable(kind) {
		if (tables[kind]) return tables[kind];
		var columns = COLUMNS[kind];
		tables[kind] = serverTable('#' + (kind === 'modular' ? 'ugcAssemblyTable' : 'ugcModelTable'), null, columns, { dataTable: {
			order: [[1, 'desc']],
			pageLength: 50,
			autoWidth: false,
			layout: { topStart: 'pageLength', topEnd: 'info', bottomStart: null, bottomEnd: 'paging' },
			language: { emptyTable: 'Nothing here.', zeroRecords: 'Nothing matches.' },
			ajax: function (data, callback) {
				var size = data.length > 0 ? data.length : 200;
				api.get('/api/ugc?' + filterQuery() + '&' + orderQuery(columns, data.order) + '&page=' + Math.floor(data.start / size) + '&size=' + size).then(function (d) {
					if (!d.success) { callback({ draw: data.draw, recordsTotal: 0, recordsFiltered: 0, data: [], error: d.error || 'Failed' }); return; }
					applyMeta(d);
					if (list.kind === kind) items = d.items;
					callback({ draw: data.draw, recordsTotal: d.total, recordsFiltered: d.total, data: d.items });
					if (tableLoaded) { tableLoaded(); tableLoaded = null; }
				}).catch(function () { callback({ draw: data.draw, recordsTotal: 0, recordsFiltered: 0, data: [] }); });
			}
		} });
		return tables[kind];
	}

	var loadSequence = 0;
	function load() {
		showControls();
		var gallery = list.view === 'gallery';
		$('gallery').classList.toggle('d-none', !gallery);
		$('pagerBar').classList.toggle('d-none', !gallery);
		$('listCard').classList.toggle('d-none', gallery);
		$('modelTableBox').classList.toggle('d-none', list.kind !== 'model');
		$('assemblyTableBox').classList.toggle('d-none', list.kind !== 'modular');
		if (!gallery) {
			return new Promise(function (resolve) {
				tableLoaded = resolve;
				var fresh = !tables[list.kind];
				var table = listTable(list.kind);
				if (!fresh) table.ajax.reload(null, true);
			});
		}
		var size = pageSize(), sequence = ++loadSequence;
		return api.get('/api/ugc?' + filterQuery() + '&sort=' + list.sort + '&page=' + list.page + '&size=' + size).then(function (d) {
			if (!d.success || sequence !== loadSequence) return;
			items = d.items;
			total = d.total;
			applyMeta(d);
			$('gallery').innerHTML = items.map(tile).join('') || '<div class="text-body-secondary">' + (list.q || list.state || list.type ? 'Nothing matches.' : 'Nothing here.') + '</div>';
			var pages = Math.max(1, Math.ceil(total / size));
			pager($('pager'), list.page, pages, function (p) { list.page = p; writeUrl(true); load(); });
			$('jumpPage').max = pages;
			$('jumpPage').placeholder = (list.page + 1) + ' / ' + pages;
			$('totalText').textContent = total.toLocaleString() + (list.kind === 'modular' ? ' assembl' + (total === 1 ? 'y' : 'ies') : ' model' + (total === 1 ? '' : 's'));
		}).catch(function () {});
	}

	function changeList(changes, push) {
		for (var k in changes) list[k] = changes[k];
		writeUrl(push);
		load();
	}

	function meter(label, value, limit, text) {
		var share = limit > 0 ? Math.min(100, value / limit * 100) : 0;
		var colour = share > 90 ? 'danger' : share > 70 ? 'warning' : 'success';
		return '<div class="col-md-4"><div class="d-flex justify-content-between"><span>' + esc(label) + '</span><span class="text-body-secondary">' + text + '</span></div>' +
			(limit > 0 ? '<div class="progress ugc-meter" role="progressbar" aria-label="' + esc(label) + '" aria-valuenow="' + Math.round(share) + '" aria-valuemin="0" aria-valuemax="100">' +
				'<div class="progress-bar bg-' + colour + '" style="width:' + share.toFixed(0) + '%"></div></div>' : '') + '</div>';
	}

	function loadStatus() {
		var box = $('serverStatus');
		api.get('/api/ugc/server/status').then(function (d) {
			if (!d.success) {
				box.innerHTML = fmt.badge('Unreachable', 'warning') + ' The dashboard can\'t reach the UGC server at ' + esc(d.url || '') + (d.error ? ' (' + esc(d.error) + ')' : '') +
					'. Is <code>enable_ugc_server</code> on, and <code>ugc_internal_url</code> (dashboard settings) right?';
				return;
			}
			var s = d.status, u = s.usage || {}, l = s.limits || {}, last = s.recent && s.recent[0];
			var cores = u.cores || 1, machinePercent = (u.cpuPercent || 0) / cores;
			var badges = fmt.badge('Online', 'success') + (u.throttled ? ' ' + fmt.badge('Throttled', 'warning') : '') + (u.paused ? ' ' + fmt.badge('Paused (' + esc(l.pauseHours) + ')', 'info') : '');
			box.innerHTML = '<div class="mb-2">' + badges + ' ' + esc(s.workers) + ' workers, ' + esc(s.active) + ' working, ' + esc(s.queued) + ' queued. ' +
				'Since it started: ' + esc(s.made) + ' made, ' + esc(s.failed) + ' failed attempts. Files: ' + esc(mb(s.storedBytes)) +
				(s.maxStorageBytes ? ' of ' + esc(mb(s.maxStorageBytes)) : '') + (s.evicted ? ', ' + esc(s.evicted) + ' deleted to save space' : '') + '.</div>' +
				'<div class="row g-3 mb-1">' +
				meter('CPU', machinePercent, l.maxCpus ? l.maxCpus / cores * 100 : 0, machinePercent.toFixed(0) + '% of ' + cores + ' cores' +
					(l.maxCpus ? ' (limit ' + (l.maxCpus / cores * 100).toFixed(0) + '%)' : ', no limit') + (u.throttledMs ? ', paused ' + (u.throttledMs / 1000).toFixed(0) + ' s to stay under it' : '')) +
				meter('Memory', u.residentBytes || 0, 0, esc(mb(u.residentBytes || 0)) + ' in use') +
				meter('Jobs\' memory', s.jobMemoryBytes || 0, l.maxMemoryBytes || 0, esc(mb(s.jobMemoryBytes || 0)) + ' estimated' +
					(l.maxMemoryBytes ? ' of ' + esc(mb(l.maxMemoryBytes)) : ', no limit') + (s.memoryWaits ? ', ' + esc(s.memoryWaits) + ' waits' : '')) +
				'</div>' + (l.nice ? '<div class="text-body-secondary">Workers run at priority ' + esc(l.nice) + '.</div>' : '') +
				purgeLine(s.purge) +
				(last ? '<div class="text-body-secondary">Last: ' + esc(last.kind) + ' ' + esc(last.id) + (last.ok ? ' made in ' + esc(duration(last.ms)) : ' failed') +
					(last.message ? ' (' + esc(last.message) + ')' : '') + '</div>' : '');
		}).catch(function () { box.innerHTML = fmt.badge('Unknown', 'secondary') + ' Couldn\'t ask the dashboard for the UGC server\'s status.'; });
	}

	function remake(body, question) {
		if (question && !confirm(question)) return;
		body.kind = list.kind;
		api.post('/api/ugc/reprocess', body).then(function (d) {
			if (!d.success) { toast(d.error || 'Failed', 'danger'); return; }
			toast(d.message, 'success');
			load();
		});
	}

	// ---- the open item ----

	function statsTable(now, before) {
		if (!now) return '<div class="small text-body-secondary">No stats for this model (made before the UGC server wrote them).</div>';
		var lods = now.lods || [], old = before && before.lods || [];
		var diff = function (a, b) {
			if (b === undefined || b === null || a === b) return '';
			var d = a - b;
			return ' <span class="' + (d < 0 ? 'text-success' : 'text-warning') + '">(' + (d > 0 ? '+' : '') + d + ')</span>';
		};
		var rows = lods.map(function (l, i) {
			var o = old[i] || {};
			var removed = l.opaqueBefore ? (100 - l.opaqueAfter / l.opaqueBefore * 100).toFixed(0) + '%' : '';
			return '<tr><td>LOD ' + esc(l.lod) + '</td><td>' + esc(l.near) + ' - ' + esc(l.far) + '</td><td>' + esc(l.opaqueBefore) + '</td><td>' + esc(l.opaqueAfter) + diff(l.opaqueAfter, o.opaqueAfter) +
				'</td><td>' + esc(removed) + '</td><td>' + esc(l.transparent) + '</td><td>' + esc(l.vertices) + diff(l.vertices, o.vertices) + '</td><td>' + esc(l.shapes) + '</td></tr>';
		}).join('');
		var ms = now.ms || {}, set = now.settings || {};
		return '<div class="table-responsive"><table class="table table-sm small mb-1"><thead><tr><th>Detail</th><th>Drawn from - to</th><th>Opaque triangles</th>' +
			'<th>After removing hidden faces</th><th>Removed</th><th>Transparent triangles</th><th>Vertices</th><th>Shapes</th></tr></thead><tbody>' + rows + '</tbody></table></div>' +
			'<div class="small text-body-secondary">' + esc(now.bricks) + ' bricks' + (now.missingDesigns ? ', no geometry for ' + esc(now.missingDesigns.join(', ')) : '') +
			'. Took ' + esc(ms.total) + ' ms (build ' + esc(ms.build) + ', hidden faces ' + esc(ms.hiddenSurfaces) + ', occlusion ' + esc(ms.ambientOcclusion) + ', icon ' + esc(ms.icon) + ')' +
			(before && before.ms ? ', before ' + esc(before.ms.total) + ' ms' : '') + '. Colors: ' + esc(set.palette) + ', variation ' + esc(set.colorVariation) + '%' +
			(set.ao ? ', occlusion ' + esc(set.aoSamples) + ' rays to ' + esc(set.aoDistance) : ', no occlusion') + '.</div>';
	}

	function loadMesh(reframe) {
		if (!current || !nifViewer) return;
		var lod = $('lodSelect').value || '0', version = $('versionSelect').value, ao = $('aoSwitch').checked ? '1' : '0';
		var stats = $('nifStats');
		stats.textContent = 'Loading…';
		nifViewer.load('/api/ugc/mesh/' + encodeURIComponent(current.id) + '?lod=' + lod + '&version=' + version + '&ao=' + ao, reframe).then(function (r) {
			var lodStats = current.stats && current.stats.lods && current.stats.lods[+lod];
			stats.textContent = r.triangles.toLocaleString() + ' triangles, ' + r.vertices.toLocaleString() + ' vertices in ' + r.shapes + ' shape' + (r.shapes === 1 ? '' : 's') +
				(lodStats && version === 'current' ? ' (' + lodStats.opaqueBefore.toLocaleString() + ' opaque triangles before hidden faces were removed)' : '');
		}).catch(function (e) { stats.textContent = 'Could not load the mesh: ' + e.message; });
	}

	function openItem(itemKind, id, build) {
		if (list.kind !== itemKind) changeList({ kind: itemKind, page: 0, sort: 'newest', type: '' }, true);
		var known = items.find(function (i) { return i.id === id; });
		if (known) return preview(known, build);
		if (itemKind === 'modular' && /^\d+$/.test(id) && id.length > 12) {
			// A build's id (links from elsewhere): open the assembly it uses, with the build shown in References
			return api.get('/api/ugc/assembly/of/' + id).then(function (d) {
				if (!d.success) { toast(d.error || 'No such build', 'warning'); return; }
				findAndOpen('modular', d.key, id);
			});
		}
		findAndOpen(itemKind, id, build);
	}
	// Not on the page showing: looked up by its id (a model's) or its modules (an assembly's)
	function findAndOpen(itemKind, id, build) {
		var q = itemKind === 'modular' ? 'lot:' + id.split('-')[0] : 'id:' + id;
		api.get('/api/ugc?kind=' + itemKind + '&q=' + encodeURIComponent(q) + '&size=200').then(function (d) {
			canManage = d.canManage;
			var found = (d.items || []).find(function (i) { return i.id === id; });
			preview(found || (itemKind === 'modular' ? { id: id, key: id, modules: id.replace(/-/g, '+'), moduleList: [], state: 'pending', uses: 0 } : { id: id, state: 'done' }), build);
		}).catch(function () {});
	}

	function modulesHtml(i) {
		return '<div class="d-flex flex-wrap gap-2 ugc-modules">' + (i.moduleList || []).map(function (m) {
			return '<div class="d-flex align-items-center gap-1 border rounded px-1"><img src="' + esc(m.icon) + '" alt="" loading="lazy" onerror="this.style.visibility=\'hidden\'">' +
				'<div><div>' + esc(m.name || 'LOT ' + m.lot) + '</div><div class="text-body-secondary">' + (DASH.can('dev_cdclient') ? '<a href="/cdclient#/object/' + esc(m.lot) + '">LOT ' + esc(m.lot) + '</a>' : 'LOT ' + esc(m.lot)) + '</div></div></div>';
		}).join('') + '</div>';
	}

	function preview(item, build) {
		var modular = list.kind === 'modular', id = item.id;
		current = { id: id, kind: list.kind, stats: null, fileId: modular ? item.iconBuild : id, item: item, build: build || '' };
		var shown = readUrlItem();
		if (!shown || shown.id !== id) writeUrl(true, { id: id, build: build });
		$('previewTitle').textContent = modular ? (item.kindLabel || 'Car or rocket') + ': ' + (moduleNames(item) || item.key) : 'Model ' + id + (item.characterName ? ' by ' + item.characterName : '');
		$('previewIcon').src = modular && !item.iconBuild ? '' : fileUrl(list.kind, current.fileId, 'icon.png');
		$('previousIconBox').classList.add('d-none');
		var previous = $('previousIcon');
		previous.onload = function () { $('previousIconBox').classList.remove('d-none'); };
		previous.src = modular && !item.iconBuild ? '' : fileUrl(list.kind, current.fileId, 'previous.icon.png');
		var links = '';
		if (!modular) {
			links += '<a href="' + esc(fileUrl('model', id, 'model.nif')) + '" download="ugc_' + esc(id) + '.nif">Download the mesh (.nif)</a><br>' +
				'<a href="/api/ugc/' + esc(id) + '/lxfml" download="ugc_' + esc(id) + '.lxfml">Download the LXFML</a><br>';
		}
		if (publicUrl && current.fileId) links += '<a href="' + esc(publicUrl + '/files/' + list.kind + '/' + current.fileId + '/icon.png') + '" target="_blank" rel="noopener">Open on the UGC server</a>';
		if (canManage && current.fileId) {
			links += '<div class="mt-2 d-flex flex-wrap gap-1"><button type="button" class="btn btn-sm btn-outline-warning" data-remake-open="' + esc(current.fileId) + '">Make again</button>' +
				'<button type="button" class="btn btn-sm btn-outline-danger" data-delete-open="now">Delete files, make again</button>' +
				'<button type="button" class="btn btn-sm btn-outline-danger" data-delete-open="gone">Delete files, leave deleted</button></div>';
		}
		if (item.error) links += '<div class="text-danger mt-2">' + esc(item.error) + '</div>';
		$('previewLinks').innerHTML = (!modular && madeText(item) ? '<div class="small text-body-secondary mb-1">Made ' + esc(madeText(item)) + '</div>' : '') + links;
		['nifColumn', 'lxfmlColumn', 'meshControls'].forEach(function (x) { $(x).classList.toggle('d-none', modular); });
		$('statsBox').innerHTML = modular ? '<div class="small mb-1">Modules (combination <code>' + esc(item.key) + '</code>, used by ' + esc(item.uses) + ' build' + (item.uses === 1 ? '' : 's') +
			'). The game client puts cars and rockets together itself; the UGC server only draws the icon, once per combination.</div>' + modulesHtml(item) : '';
		$('referencesCard').classList.toggle('d-none', !modular);
		if (modular) {
			refs = { key: item.key, page: 0, q: '', highlight: build || '' };
			$('refSearch').value = '';
			loadRefs();
		}
		UgcIconEditor.open(modular ? { model: false, modules: item.modules || item.key.replace(/-/g, '+'), label: item.key } : { model: true, id: id });
		bootstrap.Modal.getOrCreateInstance($('previewModal')).show();
		if (modular) return;

		Promise.all([fetchJson(fileUrl('model', id, 'stats.json')), fetchJson(fileUrl('model', id, 'previous.stats.json'))]).then(function (s) {
			if (!current || current.id !== id) return;
			current.stats = s[0];
			current.statsLoaded = true;
			$('statsBox').innerHTML = statsTable(s[0], s[1]);
			var lods = (s[0] && s[0].lods) || [{ lod: 0 }];
			$('lodSelect').innerHTML = lods.map(function (l, n) { return '<option value="' + n + '">LOD ' + esc(l.lod) + (l.far ? ' (' + esc(l.near) + '-' + esc(l.far) + ')' : '') + '</option>'; }).join('');
			$('versionSelect').querySelector('[value=previous]').disabled = !s[1];
			$('versionSelect').value = 'current';
			loadMesh(true);
		});
		var nifBox = $('nifViewer'), lxfmlBox = $('lxfmlViewer');
		import('/js/ugc-viewer.js').then(function (module) {
			if (!nifViewer) {
				nifViewer = module.createNifViewer(nifBox);
				nifViewer.setWireframe($('wireframeSwitch').checked);
				nifViewer.setVertexColors($('colorsSwitch').checked);
			}
			if (current && current.id === id && current.statsLoaded) loadMesh(true);
		}).catch(function (e) { nifBox.textContent = 'The 3D view could not load: ' + e.message; });
		lxfmlBox.textContent = 'Loading…';
		import('/js/lddviewer.js').then(function (module) {
			if (lxfmlViewer && lxfmlViewer.dispose) lxfmlViewer.dispose();
			lxfmlBox.textContent = '';
			lxfmlViewer = module.createViewer(lxfmlBox, {});
			return lxfmlViewer.load([{ id: id, name: 'Model ' + id, position: [0, 0, 0], rotation: [0, 0, 0, 1], url: '/api/ugc/' + id + '/lxfml' }], 1);
		}).catch(function (e) { lxfmlBox.textContent = 'The 3D view could not load: ' + e.message; });
	}
	function fetchJson(url) {
		return fetch(url, { credentials: 'same-origin' }).then(function (r) { return r.ok ? r.json() : null; }).catch(function () { return null; });
	}
	function readUrlItem() {
		var p = new URLSearchParams(location.search), item = p.get('item');
		return item ? { id: item, build: p.get('build') || '' } : null;
	}

	// ---- an assembly's references: the builds that use it, and where they are ----

	var refs = null, refTimer = null;
	function whereText(w) {
		return (w || []).map(function (x) {
			if (x.type === 'property') return 'On ' + fmt.property(x.propertyId, x.propertyName || 'property ' + x.propertyId) + (x.ownerName ? ' of ' + esc(x.ownerName) : '');
			if (x.type === 'mail') return 'In a mail to <a href="/characters/' + esc(x.characterId) + '">' + esc(x.characterName || x.characterId) + '</a>';
			return 'In the creator\'s ' + esc(x.inventory || 'inventory');
		}).join('<br>') || '<span class="text-body-secondary">Not found placed, mailed or with its creator</span>';
	}
	// The builds that use the open assembly: a server-side DataTable, made once and reloaded for each assembly. A
	// linked build (refs.highlight) opens on the page holding it, marked.
	var REF_COLUMNS = [
		column('Build (blueprint id)', function (b) { return '<code>' + esc(b.id) + '</code>'; }, 'id', true),
		column('Owner', owner, 'owner'),
		column('Account', function (b) { return b.accountId ? '<a href="/accounts/' + esc(b.accountId) + '">' + esc(b.accountName || b.accountId) + '</a>' : ''; }, 'account'),
		column('State', function (b) { return badge(b.state) + errorText(b); }, 'state'),
		column('Made', function (b) { return '<span class="small">' + esc(madeText(b)) + '</span>'; }),
		column('Where it is', function (b) { return whereText(b.where); }),
		column('', function (b) { return '<div class="text-end"><a class="btn btn-sm btn-outline-secondary" href="/ugc_search?q=' + encodeURIComponent('id:' + b.id) + '">Find</a></div>'; })
	];
	var refTable = null;
	function loadRefs() {
		if (!refs) return;
		if (refTable) { refTable.ajax.reload(null, true); return; }
		refTable = serverTable('#refTable', null, REF_COLUMNS, { dataTable: {
			order: [[0, 'desc']],
			pageLength: 25,
			autoWidth: false,
			layout: { topStart: 'pageLength', topEnd: 'info', bottomStart: null, bottomEnd: 'paging' },
			language: { emptyTable: 'No builds use these modules.', zeroRecords: 'No builds match.' },
			createdRow: function (tr, b) { if (refs && b.id === refs.highlight) tr.classList.add('ugc-highlight'); },
			ajax: function (data, callback) {
				if (!refs) { callback({ draw: data.draw, recordsTotal: 0, recordsFiltered: 0, data: [] }); return; }
				var wanted = refs, size = data.length > 0 ? data.length : 200, highlight = refs.highlight && !refs.shown ? refs.highlight : '';
				var sort = orderQuery(REF_COLUMNS, data.order).replace('sort=newest', 'sort=id');
				api.get('/api/ugc/assembly/builds?modules=' + encodeURIComponent(refs.key) + '&q=' + encodeURIComponent(refs.q) + '&' + sort +
					'&page=' + Math.floor(data.start / size) + '&size=' + size + (highlight ? '&build=' + encodeURIComponent(highlight) : '')).then(function (d) {
					if (refs !== wanted) return;
					if (!d.success) { callback({ draw: data.draw, recordsTotal: 0, recordsFiltered: 0, data: [], error: d.error || 'Failed' }); return; }
					refs.shown = true;
					callback({ draw: data.draw, recordsTotal: d.total, recordsFiltered: d.total, data: d.items });
					// The linked build is on another page: go there (the server said which)
					if (highlight && d.page * size !== data.start) setTimeout(function () { refTable.page(d.page).draw('page'); }, 0);
					var marked = document.querySelector('#refTable .ugc-highlight');
					if (marked) marked.scrollIntoView({ block: 'nearest' });
				}).catch(function () { callback({ draw: data.draw, recordsTotal: 0, recordsFiltered: 0, data: [] }); });
			}
		} });
	}
	$('refSearch').addEventListener('input', function () {
		var value = this.value.trim();
		clearTimeout(refTimer);
		refTimer = setTimeout(function () { if (refs) { refs.q = value; refs.highlight = ''; loadRefs(); } }, 300);
	});

	// ---- controls ----

	$('kindButtons').addEventListener('click', function (e) {
		var button = e.target.closest('[data-kind]');
		if (button && button.dataset.kind !== list.kind) changeList({ kind: button.dataset.kind, page: 0, sort: 'newest', type: '' }, true);
	});
	$('viewButtons').addEventListener('click', function (e) {
		var button = e.target.closest('[data-view]');
		if (!button) return;
		try { localStorage.setItem('ugcView', button.dataset.view); } catch (err) { /* storage blocked */ }
		changeList({ view: button.dataset.view, page: 0 }, false);
	});
	$('stateFilter').addEventListener('change', function () { changeList({ state: this.value, page: 0 }, true); });
	$('typeFilter').addEventListener('change', function () { changeList({ type: this.value, page: 0 }, true); });
	$('sortSelect').addEventListener('change', function () { changeList({ sort: this.value, page: 0 }, true); });
	$('pageSize').addEventListener('change', function () { Prefs.set('ugc.pageSize.' + list.view, +this.value); changeList({ page: 0 }, false); });
	$('jumpPage').addEventListener('change', function () {
		var p = Math.max(1, Math.min(+this.max || 1, parseInt(this.value, 10) || 1));
		this.value = '';
		changeList({ page: p - 1 }, true);
	});
	var searchTimer = null;
	$('search').addEventListener('input', function () {
		var value = this.value.trim();
		clearTimeout(searchTimer);
		searchTimer = setTimeout(function () { changeList({ q: value, page: 0 }, false); }, 300);
	});
	$('retryFailed').addEventListener('click', function () { remake({ failedOnly: true }); });
	$('remakeAll').addEventListener('click', function () {
		remake({}, 'Make every ' + (list.kind === 'model' ? 'model' : 'car and rocket') + ' again? This can take a long time.');
	});
	function onItemClick(e) {
		var remakeButton = e.target.closest('[data-remake]'), previewButton = e.target.closest('[data-preview]');
		if (remakeButton) remake({ id: remakeButton.dataset.remake });
		else if (previewButton) {
			var item = items.find(function (i) { return i.id === previewButton.dataset.preview; });
			if (item) preview(item);
		}
	}
	$('listCard').addEventListener('click', onItemClick);
	$('gallery').addEventListener('click', onItemClick);
	$('gallery').addEventListener('keydown', function (e) { if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); onItemClick(e); } });
	$('previewLinks').addEventListener('click', function (e) {
		var button = e.target.closest('[data-remake-open]');
		if (button) remake({ id: button.dataset.remakeOpen });
	});
	$('lodSelect').addEventListener('change', function () { loadMesh(false); });
	$('versionSelect').addEventListener('change', function () { loadMesh(false); });
	$('aoSwitch').addEventListener('change', function () { loadMesh(false); });
	$('wireframeSwitch').addEventListener('change', function () { if (nifViewer) nifViewer.setWireframe(this.checked); });
	$('colorsSwitch').addEventListener('change', function () { if (nifViewer) nifViewer.setVertexColors(this.checked); });
	$('reframeButton').addEventListener('click', function () { if (nifViewer) nifViewer.frame(); });
	$('previewModal').addEventListener('hidden.bs.modal', function () {
		current = null;
		refs = null;
		UgcIconEditor.close();
		if (lxfmlViewer && lxfmlViewer.dispose) lxfmlViewer.dispose();
		lxfmlViewer = null;
		if (readUrlItem()) writeUrl(true);
	});
	window.addEventListener('popstate', function () {
		var item = readUrl();
		load();
		if (!item) {
			if (current) bootstrap.Modal.getOrCreateInstance($('previewModal')).hide();
		} else if (!current || current.id !== item.id) {
			openItem(item.kind, item.id, item.build);
		}
	});

	// ---- deleting and purging stored files ----

	// The running or last purge (the UGC server deletes folders in the background)
	function purgeLine(p) {
		if (!p || !p.state) return '';
		var what = (p.kind === 'modular' ? 'cars and rockets' : 'models');
		var counts = esc(p.deleted) + ' deleted (' + esc(mb(p.bytes || 0)) + '), ' + esc(p.checked) + (p.total ? ' of ' + esc(p.total) : '') + ' looked at';
		if (p.state === 'done') return '<div class="text-body-secondary">Last purge (' + what + '): ' + counts + ', finished ' + esc(new Date(p.finished * 1000).toLocaleString()) + '.</div>';
		return '<div>' + fmt.badge(p.state === 'saving' ? 'Purge: updating the rows' : 'Purging', 'info') + ' ' + what + ': ' + counts + '. New items wait until it is done.</div>';
	}

	function showDeleteResult(box, d) {
		if (!d.success) { toast(d.error || 'Failed', 'danger'); return; }
		var text = d.started ? 'Deleting ' + (d.queued ? d.queued + ' item(s)' : 'every stored item') + ' in the background. ' + (d.notes || []).join(' ')
			: (d.notes && d.notes.length && !d.deleted ? d.notes.join(' ') : d.deleted + ' deleted (' + mb(d.bytes || 0) + ' freed). ' + (d.notes || []).join(' '));
		if (box) box.textContent = text;
		toast(text, 'success');
		load();
	}

	$('previewLinks').addEventListener('click', function (e) {
		var button = e.target.closest('[data-delete-open]');
		if (!button || !current) return;
		var after = button.dataset.deleteOpen;
		if (!confirm('Delete the files made for ' + current.id + (after === 'gone' ? ' and leave them deleted?' : ' and make them again?'))) return;
		api.post('/api/ugc/cache/delete', { kind: current.kind, id: current.fileId, after: after }).then(function (d) { showDeleteResult(null, d); });
	});
	function purge(all) {
		var body = { kind: list.kind, state: $('purgeState').value, owner: $('purgeOwner').value.trim(), olderThanDays: +$('purgeOlder').value || 0,
			unusedDays: +$('purgeUnused').value || 0, after: $('purgeAfter').value };
		if (all) {
			var typed = prompt('This deletes every stored ' + (list.kind === 'model' ? 'model' : 'car and rocket') + ' file. Type PURGE ALL to go ahead.');
			if (typed !== 'PURGE ALL') return;
			body = { kind: list.kind, all: true, confirm: typed, after: $('purgeAfter').value };
		} else if (!body.state && !body.owner && !body.olderThanDays && !body.unusedDays) {
			toast('Pick a filter first, or purge all', 'warning');
			return;
		} else if (!confirm('Delete the stored files of every ' + (list.kind === 'model' ? 'model' : 'car and rocket') + ' matching the filter?')) {
			return;
		}
		$('purgeResult').textContent = 'Deleting…';
		api.post('/api/ugc/cache/purge', body).then(function (d) { showDeleteResult($('purgeResult'), d); });
	}
	$('purgeButton').addEventListener('click', function () { purge(false); });
	$('purgeAllButton').addEventListener('click', function () { purge(true); });

	// ---- icon presets per type (the editor is ugc-icon-editor.js) ----

	function loadPresets() {
		UgcIconEditor.kinds().then(function (kinds) {
			$('presetsList').innerHTML = kinds.map(function (k) {
				var sample = k.sample ? (k.kind === 'model' ? 'model ' + k.sample : 'modules ' + k.sample + (k.sampleBuilds ? ' (' + k.sampleBuilds + ' builds)' : '')) : 'nothing made yet';
				return '<button type="button" class="btn btn-sm btn-outline-primary text-start" data-preset="' + esc(k.kind) + '"' + (k.sample ? '' : ' disabled') + '>' +
					'<span class="fw-semibold">' + esc(k.label) + '</span><br><span class="small text-body-secondary">on ' + esc(sample) + '</span></button>';
			}).join('');
			$('presetsList').onclick = function (e) {
				var button = e.target.closest('[data-preset]');
				var entry = button && kinds.find(function (k) { return k.kind === button.dataset.preset; });
				if (!entry || !entry.sample) return;
				openItem(entry.kind === 'model' ? 'model' : 'modular', String(entry.sample));
			};
		}).catch(function (e) { $('presetsList').textContent = 'Could not load the icon types: ' + e.message; });
	}
	loadPresets();

	if (window.Live) Live.on('ugc', load);

	var linked = readUrl();
	writeUrl(false, linked);
	load().then(function () { if (linked) openItem(linked.kind, linked.id, linked.build); });
	loadStatus();
	setInterval(loadStatus, 10000);
})();
