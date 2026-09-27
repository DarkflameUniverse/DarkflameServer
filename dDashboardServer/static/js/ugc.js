/**
 * The UGC Server page: counts, a gallery and a list of what the UGC server made of players' models and modular builds
 * (from the database), its live status, and a viewer comparing a model's generated mesh, its LXFML and its icon, now
 * and before it was made again. Everything from the UGC server comes through the dashboard (/api/ugc/server/status,
 * /api/ugc/files/..., /api/ugc/mesh/...), so the browser never has to reach the UGC server itself.
 */
(function () {
	'use strict';

	var kind = 'model', state = '', search = '', page = 0, view = 'gallery', canManage = false, publicUrl = '';
	var nifViewer = null, lxfmlViewer = null, current = null;
	var STATES = { pending: ['Waiting', 'secondary'], done: ['Made', 'success'], failed: ['Failed', 'danger'], empty: ['Empty', 'light'] };
	var PAGE_SIZE = { gallery: 48, list: 50 };

	try { view = localStorage.getItem('ugcView') === 'list' ? 'list' : 'gallery'; } catch (e) { /* storage blocked */ }

	function $(id) { return document.getElementById(id); }

	function fileUrl(itemKind, id, name) {
		return '/api/ugc/files/' + itemKind + '/' + encodeURIComponent(id) + '/' + name;
	}

	function mb(bytes) { return (bytes / 1048576).toFixed(0) + ' MB'; }

	function countCard(title, c) {
		return '<div class="col-md-6"><div class="card"><div class="card-body py-2"><div class="small text-body-secondary">' + esc(title) + '</div>' +
			fmt.badge(c.done + ' made', 'success') + ' ' + fmt.badge(c.pending + ' waiting', 'secondary') + ' ' + fmt.badge(c.failed + ' failed', c.failed ? 'danger' : 'secondary') +
			(c.empty ? ' ' + fmt.badge(c.empty + ' empty', 'light') : '') + '</div></div></div>';
	}

	function owner(i) {
		return i.characterName ? '<a href="/characters/' + esc(i.characterId) + '">' + esc(i.characterName) + '</a>' : '<span class="text-body-secondary">' + esc(i.characterId) + '</span>';
	}

	function iconImg(i, size) {
		if (i.state !== 'done') return '<span class="ugc-noicon text-body-secondary small border rounded" style="width:' + size + 'px;height:' + size + 'px">' + esc((STATES[i.state] || [i.state])[0]) + '</span>';
		return '<img src="' + esc(fileUrl(kind, i.id, 'icon.png')) + '" width="' + size + '" height="' + size + '" loading="lazy" alt="" class="ugc-checker rounded" onerror="this.style.visibility=\'hidden\'">';
	}

	function tile(i) {
		var s = STATES[i.state] || [i.state, 'secondary'];
		return '<div class="card ugc-tile p-2" tabindex="0" role="button" data-preview="' + esc(i.id) + '" title="' + esc(i.error || '') + '">' + iconImg(i, 128) +
			'<div class="small text-truncate mt-1">' + (i.characterName ? esc(i.characterName) : '<span class="text-body-secondary">' + esc(i.characterId) + '</span>') + '</div>' +
			'<div class="small text-body-secondary text-truncate">' + esc(i.id) + '</div><div>' + fmt.badge(s[0], s[1]) + extraBadges(i) + '</div></div>';
	}

	// Cars and rockets: how many builds share this icon; waiting models: when their quiet period after a save ends
	function extraBadges(i) {
		var out = '';
		if (i.sharedBy > 1) out += ' ' + fmt.badge('shared by ' + i.sharedBy, 'info');
		if (i.state === 'pending' && i.processAfter > Date.now() / 1000) out += ' <span class="small text-body-secondary" title="Waits for the owner to stop saving">after ' + esc(fmt.unix(i.processAfter)) + '</span>';
		return out;
	}

	function row(i) {
		var s = STATES[i.state] || [i.state, 'secondary'];
		var details = (kind === 'modular' ? '<code class="small">' + esc(i.modules) + '</code>' : '') + extraBadges(i);
		if (i.error) details += '<div class="small text-danger">' + esc(i.error) + '</div>';
		var buttons = '<button type="button" class="btn btn-sm btn-outline-secondary me-1" data-preview="' + esc(i.id) + '">View</button>' +
			(canManage ? '<button type="button" class="btn btn-sm btn-outline-warning" data-remake="' + esc(i.id) + '">Make again</button>' : '');
		return '<tr><td>' + iconImg(i, 48) + '</td><td class="small">' + esc(i.id) + '</td><td>' + owner(i) + '</td><td>' + fmt.badge(s[0], s[1]) +
			(i.attempts ? ' <span class="small text-body-secondary">' + esc(i.attempts) + ' attempt' + (i.attempts === 1 ? '' : 's') + '</span>' : '') + '</td>' +
			'<td class="small">' + (i.processedAt ? esc(fmt.unix(i.processedAt)) : '') + '</td><td>' + details + '</td><td class="text-end text-nowrap">' + buttons + '</td></tr>';
	}

	var items = [];
	function load() {
		var size = PAGE_SIZE[view];
		api.get('/api/ugc?kind=' + kind + '&state=' + state + '&page=' + page + '&size=' + size + '&search=' + encodeURIComponent(search)).then(function (d) {
			if (!d.success) return;
			items = d.items;
			canManage = d.canManage;
			publicUrl = (d.ugcPublicUrl || '').replace(/\/+$/, '');
			$('manageButtons').classList.toggle('d-none', !canManage);
			$('cacheCard').classList.toggle('d-none', !canManage);
			$('counts').innerHTML = countCard('Models', d.counts.model) + countCard('Cars and rockets', d.counts.modular);
			$('gallery').classList.toggle('d-none', view !== 'gallery');
			$('listCard').classList.toggle('d-none', view !== 'list');
			if (view === 'gallery') $('gallery').innerHTML = d.items.map(tile).join('') || '<div class="text-body-secondary">Nothing here.</div>';
			else $('rows').innerHTML = d.items.map(row).join('') || '<tr><td colspan="7" class="text-body-secondary">Nothing here.</td></tr>';
			$('prevPage').disabled = page === 0;
			$('nextPage').disabled = !d.more;
		}).catch(function () {});
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
				(last ? '<div class="text-body-secondary">Last: ' + esc(last.kind) + ' ' + esc(last.id) + (last.ok ? ' made in ' + esc(last.ms) + ' ms' : ' failed') +
					(last.message ? ' (' + esc(last.message) + ')' : '') + '</div>' : '');
		}).catch(function () { box.innerHTML = fmt.badge('Unknown', 'secondary') + ' Couldn\'t ask the dashboard for the UGC server\'s status.'; });
	}

	function remake(body, question) {
		if (question && !confirm(question)) return;
		body.kind = kind;
		api.post('/api/ugc/reprocess', body).then(function (d) {
			if (!d.success) { toast(d.error || 'Failed', 'danger'); return; }
			toast(d.message, 'success');
			load();
		});
	}

	// ---- the viewer ----

	function fetchJson(url) {
		return fetch(url, { credentials: 'same-origin' }).then(function (r) { return r.ok ? r.json() : null; }).catch(function () { return null; });
	}

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

	// ---- the open item in the address bar (?item=<id>&kind=model|modular), so it can be shared and Back closes it ----

	var pushedItem = false;

	function urlItem() {
		var params = new URLSearchParams(location.search), id = params.get('item');
		if (!id || !/^\d{1,20}$/.test(id)) return null;
		return { id: id, kind: params.get('kind') === 'modular' ? 'modular' : 'model' };
	}

	function setKind(value) {
		if (value === kind) return;
		kind = value;
		page = 0;
		$('kindButtons').querySelectorAll('[data-kind]').forEach(function (b) { b.classList.toggle('active', b.dataset.kind === kind); });
		load();
	}

	// Opens the item the address names, or closes the open one when it names none
	function openFromUrl() {
		var wanted = urlItem();
		if (!wanted) {
			if (current) bootstrap.Modal.getOrCreateInstance($('previewModal')).hide();
			return;
		}
		setKind(wanted.kind);
		if (current && current.id === wanted.id && current.kind === wanted.kind) return;
		var known = items.find(function (i) { return i.id === wanted.id; });
		if (known) return preview(wanted.id, known);
		// Not on the page of the list that is showing: look it up by its id
		api.get('/api/ugc?kind=' + wanted.kind + '&search=' + wanted.id + '&size=10').then(function (d) {
			canManage = d.canManage;
			publicUrl = (d.ugcPublicUrl || '').replace(/\/+$/, '');
			preview(wanted.id, (d.items || []).find(function (i) { return i.id === wanted.id; }));
		}).catch(function () { preview(wanted.id); });
	}

	function preview(id, found) {
		var item = found || items.find(function (i) { return i.id === id; }) || { id: id, state: 'done' };
		current = { id: id, kind: kind, stats: null };
		var shown = urlItem();
		if (!shown || shown.id !== id || shown.kind !== kind) {
			history.pushState(null, '', location.pathname + '?item=' + encodeURIComponent(id) + '&kind=' + kind);
			pushedItem = true;
		}
		$('previewTitle').textContent = (kind === 'model' ? 'Model ' : 'Car or rocket ') + id + (item.characterName ? ' by ' + item.characterName : '');
		$('previewIcon').src = fileUrl(kind, id, 'icon.png');
		$('previousIconBox').classList.add('d-none');
		var previous = $('previousIcon');
		previous.onload = function () { $('previousIconBox').classList.remove('d-none'); };
		previous.src = fileUrl(kind, id, 'previous.icon.png');
		var links = '';
		if (kind === 'model') {
			links += '<a href="' + esc(fileUrl('model', id, 'model.nif')) + '" download="ugc_' + esc(id) + '.nif">Download the mesh (.nif)</a><br>' +
				'<a href="/api/ugc/' + esc(id) + '/lxfml" download="ugc_' + esc(id) + '.lxfml">Download the LXFML</a><br>';
		}
		if (publicUrl) links += '<a href="' + esc(publicUrl + '/files/' + kind + '/' + id + '/icon.png') + '" target="_blank" rel="noopener">Open on the UGC server</a>';
		if (canManage) {
			links += '<div class="mt-2 d-flex flex-wrap gap-1"><button type="button" class="btn btn-sm btn-outline-warning" data-remake-open="' + esc(id) + '">Make again</button>' +
				'<button type="button" class="btn btn-sm btn-outline-danger" data-delete-open="now">Delete files, make again</button>' +
				'<button type="button" class="btn btn-sm btn-outline-danger" data-delete-open="gone">Delete files, leave deleted</button></div>';
			if (item.sharedBy > 1) links += '<div class="text-body-secondary mt-1">This icon is shared by ' + esc(item.sharedBy) + ' builds of the same modules.</div>';
		}
		if (item.error) links += '<div class="text-danger mt-2">' + esc(item.error) + '</div>';
		$('previewLinks').innerHTML = links;
		var model = kind === 'model';
		['nifColumn', 'lxfmlColumn', 'meshControls'].forEach(function (x) { $(x).classList.toggle('d-none', !model); });
		$('statsBox').innerHTML = model ? '' : '<div class="small">Modules: <code>' + esc(item.modules || '') + '</code> (combination <code>' + esc(item.combination || '') +
			'</code>). The game client puts cars and rockets together itself; only the icon is made, once per combination of modules.</div>';
		$('framingCard').classList.toggle('d-none', !canManage);
		if (canManage) openFraming(item, model);
		bootstrap.Modal.getOrCreateInstance($('previewModal')).show();
		if (!model) return;

		Promise.all([fetchJson(fileUrl('model', id, 'stats.json')), fetchJson(fileUrl('model', id, 'previous.stats.json'))]).then(function (s) {
			if (!current || current.id !== id) return;
			current.stats = s[0];
			current.statsLoaded = true;
			$('statsBox').innerHTML = statsTable(s[0], s[1]);
			var lods = (s[0] && s[0].lods) || [{ lod: 0 }];
			$('lodSelect').innerHTML = lods.map(function (l, i) { return '<option value="' + i + '">LOD ' + esc(l.lod) + (l.far ? ' (' + esc(l.near) + '-' + esc(l.far) + ')' : '') + '</option>'; }).join('');
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

	$('kindButtons').addEventListener('click', function (e) {
		var button = e.target.closest('[data-kind]');
		if (button) setKind(button.dataset.kind);
	});
	$('viewButtons').addEventListener('click', function (e) {
		var button = e.target.closest('[data-view]');
		if (!button) return;
		view = button.dataset.view;
		page = 0;
		try { localStorage.setItem('ugcView', view); } catch (err) { /* storage blocked */ }
		this.querySelectorAll('[data-view]').forEach(function (b) { b.classList.toggle('active', b === button); });
		load();
	});
	$('viewButtons').querySelectorAll('[data-view]').forEach(function (b) { b.classList.toggle('active', b.dataset.view === view); });
	$('stateFilter').addEventListener('change', function () { state = this.value; page = 0; load(); });
	var searchTimer = null;
	$('search').addEventListener('input', function () {
		var value = this.value.trim();
		clearTimeout(searchTimer);
		searchTimer = setTimeout(function () { search = value; page = 0; load(); }, 300);
	});
	$('prevPage').addEventListener('click', function () { if (page > 0) { page--; load(); } });
	$('nextPage').addEventListener('click', function () { page++; load(); });
	$('retryFailed').addEventListener('click', function () { remake({ failedOnly: true }); });
	$('remakeAll').addEventListener('click', function () {
		remake({}, 'Make every ' + (kind === 'model' ? 'model' : 'car and rocket') + ' again? This can take a long time.');
	});
	function onItemClick(e) {
		var remakeButton = e.target.closest('[data-remake]'), previewButton = e.target.closest('[data-preview]');
		if (remakeButton) remake({ id: remakeButton.dataset.remake });
		else if (previewButton) preview(previewButton.dataset.preview);
	}
	$('rows').addEventListener('click', onItemClick);
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
		if (lxfmlViewer && lxfmlViewer.dispose) lxfmlViewer.dispose();
		lxfmlViewer = null;
		// Closed here rather than with Back: take the item out of the address too
		if (urlItem()) {
			if (pushedItem) history.back();
			else history.replaceState(null, '', location.pathname);
		}
		pushedItem = false;
	});
	window.addEventListener('popstate', function () {
		pushedItem = false;
		openFromUrl();
	});
	// ---- deleting and purging stored files ----

	function showDeleteResult(box, d) {
		if (!d.success) { toast(d.error || 'Failed', 'danger'); return; }
		var text = d.deleted + ' deleted (' + mb(d.bytes || 0) + ' freed). ' + (d.notes || []).join(' ');
		if (box) box.textContent = text;
		toast(text, 'success');
		load();
	}

	$('previewLinks').addEventListener('click', function (e) {
		var button = e.target.closest('[data-delete-open]');
		if (!button || !current) return;
		var after = button.dataset.deleteOpen;
		if (!confirm('Delete the files made for ' + current.id + (after === 'gone' ? ' and leave them deleted?' : ' and make them again?'))) return;
		api.post('/api/ugc/cache/delete', { kind: current.kind, id: current.id, after: after }).then(function (d) { showDeleteResult(null, d); });
	});
	function purge(all) {
		var body = { kind: kind, state: $('purgeState').value, owner: $('purgeOwner').value.trim(), olderThanDays: +$('purgeOlder').value || 0,
			unusedDays: +$('purgeUnused').value || 0, after: $('purgeAfter').value };
		if (all) {
			var typed = prompt('This deletes every stored ' + (kind === 'model' ? 'model' : 'car and rocket') + ' file. Type PURGE ALL to go ahead.');
			if (typed !== 'PURGE ALL') return;
			body = { kind: kind, all: true, confirm: typed, after: $('purgeAfter').value };
		} else if (!body.state && !body.owner && !body.olderThanDays && !body.unusedDays) {
			toast('Pick a filter first, or purge all', 'warning');
			return;
		} else if (!confirm('Delete the stored files of every ' + (kind === 'model' ? 'model' : 'car and rocket') + ' matching the filter?')) {
			return;
		}
		$('purgeResult').textContent = 'Deleting…';
		api.post('/api/ugc/cache/purge', body).then(function (d) { showDeleteResult($('purgeResult'), d); });
	}
	$('purgeButton').addEventListener('click', function () { purge(false); });
	$('purgeAllButton').addEventListener('click', function () { purge(true); });

	// ---- icon framing and light, for any item; the controls come from the server's list of parameters ----

	var PARAMS = [], framingItem = null, framingKind = '', previewTimer = null, previewUrl = null;
	function stepDigits(step) { return step >= 1 ? 0 : 2; }
	function buildControls(params) {
		PARAMS = params;
		$('framingControls').innerHTML = params.map(function (p) {
			return '<div class="col-sm-6"><label class="d-flex justify-content-between" for="icon_' + esc(p.key) + '" title="' + esc(p.description || '') + '"><span>' + esc(p.label) +
				'</span><span id="icon_' + esc(p.key) + '_value"></span></label><input type="range" class="form-range" id="icon_' + esc(p.key) + '" min="' + p.min + '" max="' + p.max + '" step="' + p.step + '"></div>';
		}).join('');
	}
	function framingValues() {
		var out = {};
		PARAMS.forEach(function (p) { out[p.key] = parseFloat($('icon_' + p.key).value); });
		return out;
	}
	function setValues(values) {
		if (!values) return;
		PARAMS.forEach(function (p) {
			if (values[p.key] === undefined || values[p.key] === null) return;
			$('icon_' + p.key).value = values[p.key];
			$('icon_' + p.key + '_value').textContent = (+values[p.key]).toFixed(stepDigits(p.step)) + (p.unit === 'degrees' ? '\u00b0' : '');
		});
	}
	function itemQuery() {
		return framingItem.model ? 'kind=model&id=' + encodeURIComponent(framingItem.id) : 'kind=modular&modules=' + encodeURIComponent(framingItem.modules || '');
	}
	function itemBody(extra) {
		var body = framingItem.model ? { kind: 'model', id: framingItem.id } : { kind: 'modular', modules: framingItem.modules };
		for (var k in extra) body[k] = extra[k];
		return body;
	}
	function loadValues() {
		return api.get('/api/ugc/icon/settings?' + itemQuery()).then(function (d) {
			if (!d.success) { $('framingState').textContent = d.error || 'Failed'; return; }
			framingKind = d.kind;
			setValues(d.settings);
			setValues(d.preset);
			setValues(d.own);
			$('framingState').textContent = d.own ? 'This item has its own values.' : d.preset ? 'The kind\'s preset.' : 'The default settings.';
			renderPreview();
		});
	}
	function openFraming(item, model) {
		framingItem = { id: item.id, modules: item.modules, model: model };
		var start = PARAMS.length ? Promise.resolve() : api.get('/api/ugc/icon/params').then(function (d) {
			if (!d.success) return;
			buildControls(d.params);
			window.ugcIconKinds = d.kinds;
		});
		start.then(loadValues).then(function () {
			var label = (window.ugcIconKinds || []).find(function (k) { return k.kind === framingKind; });
			$('framingKind').textContent = label ? label.label : framingKind;
		});
	}
	function renderPreview() {
		if (!framingItem) return;
		var img = $('framingPreview');
		img.style.opacity = 0.5;
		fetch('/api/ugc/icon/preview', { method: 'POST', credentials: 'same-origin', headers: { 'Content-Type': 'application/json', 'X-Requested-With': 'XMLHttpRequest' },
			body: JSON.stringify(itemBody({ values: framingValues() })) }).then(function (r) {
			if (!r.ok) return r.json().then(function (d) { throw new Error(d.error || 'HTTP ' + r.status); });
			return r.blob();
		}).then(function (blob) {
			if (previewUrl) URL.revokeObjectURL(previewUrl);
			previewUrl = URL.createObjectURL(blob);
			img.src = previewUrl;
			img.style.opacity = 1;
		}).catch(function (e) { img.style.opacity = 1; $('framingState').textContent = 'No preview: ' + e.message; });
	}
	function saved(d) { toast(d.success ? d.message : (d.error || 'Failed'), d.success ? 'success' : 'danger'); }
	$('framingControls').addEventListener('input', function (e) {
		var p = PARAMS.find(function (x) { return 'icon_' + x.key === e.target.id; });
		if (p) $('icon_' + p.key + '_value').textContent = (+e.target.value).toFixed(stepDigits(p.step)) + (p.unit === 'degrees' ? '\u00b0' : '');
		clearTimeout(previewTimer);
		previewTimer = setTimeout(renderPreview, 350);
	});
	$('framingPreviewButton').addEventListener('click', renderPreview);
	$('framingReset').addEventListener('click', function () {
		if (!framingItem || !confirm('Remove this item\'s own values and go back to its kind\'s?')) return;
		api.post('/api/ugc/icon/save', itemBody({ scope: 'item', values: null })).then(function (d) { saved(d); loadValues(); });
	});
	$('framingSaveType').addEventListener('click', function () {
		if (!confirm('Use these values for every icon of this kind? Icons already made keep theirs until they are drawn again.')) return;
		api.post('/api/ugc/icon/save', { scope: 'kind', kind: framingKind, values: framingValues() }).then(saved);
	});
	$('framingSaveCombo').addEventListener('click', function () {
		if (!framingItem) return;
		api.post('/api/ugc/icon/save', itemBody({ scope: 'item', values: framingValues() })).then(function (d) {
			saved(d);
			if (d.success) $('framingState').textContent = 'This item has its own values.';
		});
	});
	$('framingRegenerate').addEventListener('click', function () {
		if (!confirm('Draw every stored icon of this kind again with the saved values? (Only icons are drawn.)')) return;
		api.post('/api/ugc/icon/regenerate', { kind: framingKind }).then(function (d) {
			toast(d.success ? d.queued + ' icon' + (d.queued === 1 ? '' : 's') + ' queued' : (d.error || 'Failed'), d.success ? 'success' : 'danger');
		});
	});

	if (window.Live) Live.on('ugc', load);

	var linked = urlItem();
	if (linked) kind = linked.kind;
	$('kindButtons').querySelectorAll('[data-kind]').forEach(function (b) { b.classList.toggle('active', b.dataset.kind === kind); });
	load();
	if (linked) openFromUrl();
	loadStatus();
	setInterval(loadStatus, 10000);
})();
