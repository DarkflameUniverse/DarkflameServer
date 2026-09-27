/**
 * The UGC Server page: counts and a list of what the UGC server made of players' models and modular builds (from the
 * database), its live status and previews (from the UGC server itself, at ugc_public_url), and making things again.
 */
(function () {
	'use strict';

	var kind = 'model', state = '', page = 0, ugcUrl = '', canManage = false, viewer = null;
	var STATES = { pending: ['Waiting', 'secondary'], done: ['Made', 'success'], failed: ['Failed', 'danger'] };

	function serverUrl() {
		return (ugcUrl || (location.protocol + '//' + location.hostname + ':2008')).replace(/\/+$/, '');
	}

	function fileUrl(id, name) {
		return serverUrl() + '/files/' + kind + '/' + encodeURIComponent(id) + '/' + name;
	}

	function countCard(title, c) {
		return '<div class="col-md-6"><div class="card"><div class="card-body py-2"><div class="small text-body-secondary">' + esc(title) + '</div>' +
			fmt.badge(c.done + ' made', 'success') + ' ' + fmt.badge(c.pending + ' waiting', 'secondary') + ' ' + fmt.badge(c.failed + ' failed', c.failed ? 'danger' : 'secondary') +
			'</div></div></div>';
	}

	function row(i) {
		var s = STATES[i.state] || [i.state, 'secondary'];
		var icon = i.state === 'done' ? '<img src="' + esc(fileUrl(i.id, 'icon.png')) + '" width="48" height="48" loading="lazy" alt="" onerror="this.style.visibility=\'hidden\'">' : '';
		var owner = i.characterName ? '<a href="/characters/' + esc(i.characterId) + '">' + esc(i.characterName) + '</a>' : '<span class="text-body-secondary">' + esc(i.characterId) + '</span>';
		var details = kind === 'modular' ? '<code class="small">' + esc(i.modules) + '</code>' : '';
		if (i.error) details += '<div class="small text-danger">' + esc(i.error) + '</div>';
		var buttons = (kind === 'model' ? '<button type="button" class="btn btn-sm btn-outline-secondary me-1" data-preview="' + esc(i.id) + '">View</button>' : '') +
			(canManage ? '<button type="button" class="btn btn-sm btn-outline-warning" data-remake="' + esc(i.id) + '">Make again</button>' : '');
		return '<tr><td>' + icon + '</td><td class="small">' + esc(i.id) + '</td><td>' + owner + '</td><td>' + fmt.badge(s[0], s[1]) +
			(i.attempts ? ' <span class="small text-body-secondary">' + esc(i.attempts) + ' attempt' + (i.attempts === 1 ? '' : 's') + '</span>' : '') + '</td>' +
			'<td class="small">' + (i.processedAt ? esc(fmt.unix(i.processedAt)) : '') + '</td><td>' + details + '</td><td class="text-end text-nowrap">' + buttons + '</td></tr>';
	}

	function load() {
		api.get('/api/ugc?kind=' + kind + '&state=' + state + '&page=' + page).then(function (d) {
			if (!d.success) return;
			ugcUrl = d.ugcUrl;
			canManage = d.canManage;
			document.getElementById('manageButtons').classList.toggle('d-none', !canManage);
			document.getElementById('counts').innerHTML = countCard('Models', d.counts.model) + countCard('Cars and rockets', d.counts.modular);
			document.getElementById('rows').innerHTML = d.items.map(row).join('') || '<tr><td colspan="7" class="text-body-secondary">Nothing here.</td></tr>';
			document.getElementById('prevPage').disabled = page === 0;
			document.getElementById('nextPage').disabled = !d.more;
			loadStatus();
		}).catch(function () {});
	}

	function loadStatus() {
		var box = document.getElementById('serverStatus');
		fetch(serverUrl() + '/status').then(function (r) { return r.json(); }).then(function (s) {
			var mb = function (b) { return (b / 1048576).toFixed(0) + ' MB'; };
			var last = s.recent && s.recent[0];
			box.innerHTML = fmt.badge('Online', 'success') + ' ' + esc(s.workers) + ' workers, ' + esc(s.active) + ' working, ' + esc(s.queued) + ' queued. ' +
				'Since it started: ' + esc(s.made) + ' made, ' + esc(s.failed) + ' failed attempts. Files: ' + esc(mb(s.storedBytes)) +
				(s.maxStorageBytes ? ' of ' + esc(mb(s.maxStorageBytes)) : '') + (s.evicted ? ', ' + esc(s.evicted) + ' deleted to save space' : '') + '.' +
				(last ? '<div class="text-body-secondary">Last: ' + esc(last.kind) + ' ' + esc(last.id) + (last.ok ? ' made in ' + esc(last.ms) + ' ms' : ' failed') +
					(last.message ? ' (' + esc(last.message) + ')' : '') + '</div>' : '');
		}).catch(function () {
			box.innerHTML = fmt.badge('Unreachable', 'warning') + ' The UGC server doesn\'t answer at ' + esc(serverUrl()) +
				'. Is <code>enable_ugc_server</code> on, and <code>ugc_public_url</code> right?';
		});
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

	function preview(id) {
		document.getElementById('previewTitle').textContent = 'Model ' + id;
		document.getElementById('previewIcon').src = fileUrl(id, 'icon.png');
		document.getElementById('previewLinks').innerHTML = '<a href="' + esc(fileUrl(id, 'model.nif')) + '">Download the mesh (.nif)</a><br>' +
			'<a href="/api/ugc/' + esc(id) + '/lxfml" download="ugc_' + esc(id) + '.lxfml">Download the LXFML</a>';
		bootstrap.Modal.getOrCreateInstance(document.getElementById('previewModal')).show();
		var container = document.getElementById('previewViewer');
		container.textContent = 'Loading…';
		import('/js/lddviewer.js').then(function (module) {
			if (viewer && viewer.dispose) viewer.dispose();
			container.textContent = '';
			viewer = module.createViewer(container, {});
			return viewer.load([{ id: id, name: 'Model ' + id, position: [0, 0, 0], rotation: [0, 0, 0, 1], url: '/api/ugc/' + id + '/lxfml' }], 1);
		}).catch(function (e) { container.textContent = 'The 3D view could not load: ' + e.message; });
	}

	document.getElementById('kindButtons').addEventListener('click', function (e) {
		var button = e.target.closest('[data-kind]');
		if (!button) return;
		kind = button.dataset.kind;
		page = 0;
		this.querySelectorAll('[data-kind]').forEach(function (b) { b.classList.toggle('active', b === button); });
		load();
	});
	document.getElementById('stateFilter').addEventListener('change', function () { state = this.value; page = 0; load(); });
	document.getElementById('prevPage').addEventListener('click', function () { if (page > 0) { page--; load(); } });
	document.getElementById('nextPage').addEventListener('click', function () { page++; load(); });
	document.getElementById('retryFailed').addEventListener('click', function () { remake({ failedOnly: true }); });
	document.getElementById('remakeAll').addEventListener('click', function () {
		remake({}, 'Make every ' + (kind === 'model' ? 'model' : 'car and rocket') + ' again? This can take a long time.');
	});
	document.getElementById('rows').addEventListener('click', function (e) {
		var remakeButton = e.target.closest('[data-remake]'), previewButton = e.target.closest('[data-preview]');
		if (remakeButton) remake({ id: remakeButton.dataset.remake });
		else if (previewButton) preview(previewButton.dataset.preview);
	});
	if (window.Live) Live.on('ugc', load);

	load();
	setInterval(loadStatus, 10000);
})();
