/**
 * Character page: missions (with completing, resetting and giving one for staff), progress (completion per group and
 * zone, compared with the server) and giving back what was lost since a snapshot. Names, states, groups and the
 * possible changes all come from the server.
 */
(function () {
	'use strict';
	var config = window.characterToolsConfig;
	if (!config) return;
	var nf = new Intl.NumberFormat();
	var canChange = !!config.canMissions;

	function count(c) { return nf.format(c.done) + ' / ' + nf.format(c.available); }
	function percent(c) { return c.available ? Math.round(100 * c.done / c.available) : 0; }
	function bar(c) {
		return '<div class="progress mt-1" style="height: 6px;" role="progressbar" aria-valuenow="' + percent(c) + '" aria-valuemin="0" aria-valuemax="100">' +
			'<div class="progress-bar" style="width: ' + percent(c) + '%"></div></div>';
	}
	function group(m) { return [m.type, m.subtype].filter(Boolean).join(' / '); }

	// ---- missions ----
	var missions = [], changes = [], found = [];

	function taskList(m) {
		var tasks = m.tasks.map(function (t) {
			return '<li>' + esc(t.description || t.type) + ' <span class="text-body-secondary">' + nf.format(t.progress) + ' / ' + nf.format(t.target) + '</span></li>';
		}).join('');
		var r = m.rewards, rewards = [];
		if (r) {
			if (r.coins) rewards.push(nf.format(r.coins) + ' coins');
			if (r.uscore) rewards.push(nf.format(r.uscore) + ' U-score');
			if (r.reputation) rewards.push(nf.format(r.reputation) + ' reputation');
			r.items.forEach(function (i) { rewards.push(nf.format(i.count) + 'x ' + (i.name || 'LOT ' + i.lot)); });
		}
		return (tasks ? '<ul class="small mb-1">' + tasks + '</ul>' : '') +
			(rewards.length ? '<div class="small text-body-secondary">Rewards: ' + esc(rewards.join(', ')) + '</div>' : '');
	}

	function changeButtons(id) {
		if (!canChange) return '';
		return '<div class="btn-group btn-group-sm">' + changes.map(function (c) {
			return '<button type="button" class="btn btn-outline-secondary" data-mission="' + esc(id) + '" data-change="' + esc(c.value) + '">' + esc(c.name) + '</button>';
		}).join('') + '</div>';
	}

	function renderMissions() {
		var text = document.getElementById('missionFilter').value.trim().toLowerCase();
		var kind = document.getElementById('missionKind').value;
		var show = document.getElementById('missionShow').value;
		var rows = missions.filter(function (m) {
			if (kind && m.kind !== kind) return false;
			if (show === 'current' && !m.current) return false;
			if (show === 'done' && !m.done) return false;
			return !text || (m.name + ' ' + m.id + ' ' + group(m)).toLowerCase().indexOf(text) !== -1;
		}).map(function (m) {
			return '<tr><td data-label="Mission"><div class="fw-semibold">' + esc(m.name) + ' <span class="text-body-secondary small">' + esc(m.id) + '</span></div>' +
				'<div class="small text-body-secondary">' + esc(m.kind) + (group(m) ? ' &middot; ' + esc(group(m)) : '') + '</div>' + taskList(m) + '</td>' +
				'<td data-label="State">' + fmt.badge(m.state_name, m.done && !m.current ? 'success' : 'secondary') +
				(m.completions ? '<div class="small text-body-secondary">' + nf.format(m.completions) + 'x, last ' + esc(fmt.unix(m.completed_at)) + '</div>' : '') + '</td>' +
				(canChange ? '<td class="text-end">' + changeButtons(m.id) + '</td>' : '') + '</tr>';
		});
		document.getElementById('missionRows').innerHTML = rows.join('') || '<tr><td colspan="3" class="text-body-secondary">No missions match</td></tr>';
	}

	function loadMissions() {
		return api.get('/api/characters/' + config.id + '/missions').then(function (d) {
			if (!d.success) { toast(d.error || 'Could not read the missions', 'danger'); return; }
			missions = d.missions.sort(function (a, b) { return (b.completed_at || 0) - (a.completed_at || 0) || a.id - b.id; });
			changes = d.changes;
			var kinds = [];
			missions.forEach(function (m) { if (kinds.indexOf(m.kind) === -1) kinds.push(m.kind); });
			var select = document.getElementById('missionKind'), current = select.value;
			select.innerHTML = '<option value="">Missions and achievements</option>' + kinds.map(function (k) { return '<option>' + esc(k) + '</option>'; }).join('');
			select.value = current;
			renderMissions();
		});
	}

	function changeMission(id, change) {
		var name = (missions.concat(found).filter(function (m) { return String(m.id) === String(id); })[0] || {}).name || 'mission ' + id;
		var info = changes.filter(function (c) { return c.value === change; })[0] || {};
		var rewards = !!info.rewards && document.getElementById('missionRewards').checked;
		if (!confirm((info.name || change) + ' "' + name + '"' + (info.rewards ? (rewards ? ' with its rewards' : ' without its rewards') : '') + '?')) return;
		api.action('/api/characters/' + config.id + '/missions/' + id, { change: change, rewards: rewards }).then(function (d) { return d.result; }).then(function (r) {
			if (r && r.success) loadMissions();
		}).catch(function () {});
	}

	var searchTimer = null;
	function wireMissions() {
		var button = document.getElementById('missionsBtn');
		if (!button) return;
		button.addEventListener('click', function () {
			bootstrap.Modal.getOrCreateInstance(document.getElementById('missionsModal')).show();
			loadMissions();
		});
		['missionFilter', 'missionKind', 'missionShow'].forEach(function (id) { document.getElementById(id).addEventListener('input', renderMissions); });
		document.getElementById('missionsModal').addEventListener('click', function (e) {
			var b = e.target.closest('[data-change]');
			if (b) changeMission(b.dataset.mission, b.dataset.change);
		});
		var search = document.getElementById('missionSearch');
		if (!search) return;
		search.addEventListener('input', function () {
			clearTimeout(searchTimer);
			var q = search.value.trim();
			searchTimer = setTimeout(function () {
				if (q.length < 2 && !/^\d+$/.test(q)) { document.getElementById('missionSearchResults').innerHTML = ''; return; }
				api.get('/api/missions?q=' + encodeURIComponent(q)).then(function (d) {
					found = d.missions || [];
					document.getElementById('missionSearchResults').innerHTML = found.map(function (m) {
						return '<li class="list-group-item d-flex flex-wrap gap-2 align-items-center"><span class="me-auto">' + esc(m.name) + ' <span class="text-body-secondary small">' +
							esc(m.id) + ' &middot; ' + esc(m.kind) + (group(m) ? ' &middot; ' + esc(group(m)) : '') + '</span></span>' + changeButtons(m.id) + '</li>';
					}).join('') || '<li class="list-group-item text-body-secondary">Nothing found</li>';
				});
			}, 250);
		});
	}

	// ---- progress ----
	var progressRetries = 0;
	function standing(label, c, s) {
		return '<div class="col-md-6"><div class="card h-100"><div class="card-body">' +
			'<div class="text-body-secondary small text-uppercase">' + esc(label) + '</div>' +
			'<div class="fs-4 fw-bold">' + count(c) + ' <span class="fs-6 text-body-secondary">(' + percent(c) + '%)</span></div>' + bar(c) +
			(s ? '<div class="small mt-2">Server average ' + nf.format(Math.round(s.average)) + '; ahead of ' + Math.round(s.percentile) + '% of characters</div>' : '') +
			'</div></div></div>';
	}

	function renderProgress(d) {
		var p = d.progress, box = document.getElementById('progressBody');
		var html = '<div class="row g-3 mb-3">' + standing('Achievements', p.achievements, p.average && p.average.achievements) + standing('Missions', p.missions, p.average && p.average.missions) + '</div>';
		if (p.average) html += '<p class="small text-body-secondary">Compared with the ' + nf.format(p.average.characters) + ' characters that have finished anything, as of ' + esc(fmt.unix(p.average.updated)) + '.</p>';
		else if (d.average_pending) html += '<p class="small text-body-secondary">Working out the server\'s averages&hellip;</p>';

		html += '<h6>Zones</h6><div class="table-responsive"><table class="table table-sm align-middle table-stack"><thead><tr><th>Zone</th><th>Missions</th><th>Achievements</th><th>To find</th><th>Earned there</th></tr></thead><tbody>';
		p.zones.forEach(function (z) {
			var c = z.category !== null ? p.categories[z.category] : null;
			var summary = z.summary.map(function (s) { return '<div>' + esc(s.name) + ': ' + nf.format(s.found) + ' / ' + nf.format(s.total) + (s.done ? ' ' + fmt.badge('done', 'success') : '') + '</div>'; }).join('');
			var stats = z.stats ? [nf.format(z.stats.achievements) + ' achievements', nf.format(z.stats.coins) + ' coins', nf.format(z.stats.enemies) + ' enemies', nf.format(z.stats.quickbuilds) + ' quick builds'].join(', ') : '';
			html += '<tr><td data-label="Zone">' + esc(z.name) + (z.visited ? '' : ' ' + fmt.badge('not visited', 'secondary')) + '</td>' +
				'<td data-label="Missions">' + (c ? count(c.missions) + bar(c.missions) : '') + '</td>' +
				'<td data-label="Achievements">' + (c ? count(c.achievements) + bar(c.achievements) : '') + '</td>' +
				'<td data-label="To find" class="small">' + summary + '</td><td data-label="Earned there" class="small">' + stats + '</td></tr>';
		});
		html += '</tbody></table></div>';

		html += '<h6>All groups</h6><div class="table-responsive"><table class="table table-sm align-middle table-stack"><thead><tr><th>Group</th><th>Missions</th><th>Achievements</th>' +
			(p.average ? '<th>Server average</th>' : '') + '</tr></thead><tbody>';
		p.categories.slice().sort(function (a, b) { return group(a).localeCompare(group(b)); }).forEach(function (c) {
			html += '<tr><td data-label="Group">' + esc(group(c) || '-') + '</td><td data-label="Missions">' + (c.missions.available ? count(c.missions) : '') + '</td>' +
				'<td data-label="Achievements">' + (c.achievements.available ? count(c.achievements) : '') + '</td>' +
				(p.average ? '<td data-label="Server average" class="small">' + [c.missions.available ? nf.format(Math.round(c.average.missions)) + ' missions' : '',
					c.achievements.available ? nf.format(Math.round(c.average.achievements)) + ' achievements' : ''].filter(Boolean).join(', ') + '</td>' : '') + '</tr>';
		});
		box.innerHTML = html + '</tbody></table></div>';
	}

	function loadProgress() {
		api.get('/api/characters/' + config.id + '/progress').then(function (d) {
			if (!d.success) { document.getElementById('progressBody').innerHTML = '<p class="text-danger">' + esc(d.error) + '</p>'; return; }
			renderProgress(d);
			// Averages come from reading every character; ask again once they're ready
			if (d.average_pending && progressRetries++ < 10) setTimeout(loadProgress, 3000);
		});
	}

	function wireProgress() {
		var button = document.getElementById('progressBtn');
		if (!button) return;
		button.addEventListener('click', function () {
			bootstrap.Modal.getOrCreateInstance(document.getElementById('progressModal')).show();
			progressRetries = 0;
			loadProgress();
		});
	}

	// ---- giving back lost items ----
	var restoreSnapshot = null, missing = [];

	function renderRestore(data) {
		missing = data.missing;
		var box = document.getElementById('restoreCompare');
		var restorable = missing.filter(function (m) { return m.restorable > 0; });
		var html = '<div class="fw-semibold mb-2">' + esc(fmt.unix(data.taken_at)) + ' &rarr; now</div>';
		var sets = data.sets.filter(function (s) { return s.lots.some(function (lot) { return restorable.some(function (m) { return m.lot === lot; }); }); });
		if (sets.length) html += '<div class="d-flex flex-wrap gap-2 mb-2">' + sets.map(function (s) {
			return '<button type="button" class="btn btn-sm btn-outline-primary" data-set="' + esc(s.id) + '">Select the ' + esc(s.name || 'item set') + ' set</button>';
		}).join('') + '</div>';
		html += '<div class="table-responsive"><table class="table table-sm align-middle table-stack"><thead><tr><th></th><th>Item</th><th>Then</th><th>Now</th><th>Accounted for</th><th style="width: 7rem">Give back</th></tr></thead><tbody>' +
			missing.map(function (m) {
				var accounted = [];
				if (m.mailed) accounted.push(nf.format(m.mailed) + ' in their mailbox');
				if (m.elsewhere) accounted.push(nf.format(m.elsewhere) + ' someone else has');
				var disabled = m.restorable ? '' : ' disabled';
				return '<tr><td><input class="form-check-input" type="checkbox" data-pick="' + esc(m.lot) + '"' + disabled + ' aria-label="Give back ' + esc(m.name) + '"></td>' +
					'<td data-label="Item">' + esc(m.name || 'LOT ' + m.lot) + ' <span class="text-body-secondary small">' + esc(m.lot) + '</span></td>' +
					'<td data-label="Then">' + nf.format(m.then) + '</td><td data-label="Now">' + nf.format(m.now) + '</td>' +
					'<td data-label="Accounted for" class="small">' + esc(accounted.join(', ')) + '</td>' +
					'<td data-label="Give back"><input type="number" class="form-control form-control-sm" min="1" max="' + esc(m.restorable) + '" value="' + esc(m.restorable) + '" data-count="' + esc(m.lot) + '"' + disabled + '></td></tr>';
			}).join('') + '</tbody></table></div>';
		if (!missing.length) html = '<p class="text-body-secondary">Nothing the character had then is missing now.</p>';

		var byInventory = {};
		data.changes.forEach(function (c) { (byInventory[c.inventory] = byInventory[c.inventory] || []).push(c); });
		html += '<details class="mt-2"><summary class="small">Every change, per inventory</summary>' + Object.keys(byInventory).map(function (type) {
			return '<div class="fw-semibold small mt-2">' + esc(Labels.name('inventories', type) || type) + '</div><ul class="small mb-0">' + byInventory[type].map(function (c) {
				return '<li class="' + (c.now < c.then ? 'text-danger' : 'text-success') + '">' + esc(c.name || 'LOT ' + c.lot) + ': ' + nf.format(c.then) + ' &rarr; ' + nf.format(c.now) + '</li>';
			}).join('') + '</ul>';
		}).join('') + '</details>';
		box.innerHTML = html;
		document.getElementById('restoreSend').disabled = !restorable.length;
	}

	function openRestoreSnapshot(id) {
		restoreSnapshot = id;
		document.querySelectorAll('#restoreSnapshots [data-snapshot]').forEach(function (el) { el.classList.toggle('active', el.dataset.snapshot === String(id)); });
		var box = document.getElementById('restoreCompare');
		box.innerHTML = '<p class="text-body-secondary">Comparing with everyone\'s items and mail&hellip;</p>';
		api.job('/api/characters/' + config.id + '/restore/' + id, null, 'GET').then(renderRestore).catch(function (e) { box.innerHTML = '<p class="text-danger">' + esc(e.message) + '</p>'; });
	}

	function wireRestore() {
		var button = document.getElementById('restoreBtn');
		if (!button) return;
		button.addEventListener('click', function () {
			bootstrap.Modal.getOrCreateInstance(document.getElementById('restoreModal')).show();
			api.get('/api/characters/' + config.id + '/snapshots').then(function (d) {
				var list = document.getElementById('restoreSnapshots');
				if (!d.success) { list.innerHTML = '<div class="text-danger">' + esc(d.error) + '</div>'; return; }
				list.innerHTML = d.snapshots.map(function (s) {
					return '<a href="#" class="list-group-item list-group-item-action" data-snapshot="' + s.id + '"><div class="fw-semibold">' + esc(fmt.unix(s.taken_at)) + '</div>' +
						'<div class="text-body-secondary">' + esc(s.reason) + (s.actor ? ' by ' + esc(s.actor) : '') + '</div></a>';
				}).join('') || '<div class="text-body-secondary">No snapshots yet.</div>';
			});
		});
		document.getElementById('restoreSnapshots').addEventListener('click', function (e) {
			var item = e.target.closest('[data-snapshot]');
			if (item) { e.preventDefault(); openRestoreSnapshot(item.dataset.snapshot); }
		});
		document.getElementById('restoreCompare').addEventListener('click', function (e) {
			var set = e.target.closest('[data-set]');
			if (!set) return;
			var lots = []; // every missing piece of the set
			document.querySelectorAll('#restoreCompare [data-pick]').forEach(function (box) {
				var m = missing.filter(function (x) { return String(x.lot) === box.dataset.pick; })[0];
				if (m && String(m.set) === set.dataset.set && !box.disabled) { box.checked = true; lots.push(m.lot); }
			});
			if (!lots.length) toast('Nothing of that set can be given back', 'warning');
		});
		document.getElementById('restoreSend').addEventListener('click', function () {
			var items = [];
			document.querySelectorAll('#restoreCompare [data-pick]:checked').forEach(function (box) {
				var input = document.querySelector('#restoreCompare [data-count="' + box.dataset.pick + '"]');
				items.push({ lot: Number(box.dataset.pick), count: Number(input.value) || 0 });
			});
			if (!items.length) { toast('Tick the items to give back', 'warning'); return; }
			if (!confirm('Mail ' + items.length + ' kind(s) of item back to this character?')) return;
			api.action('/api/characters/' + config.id + '/restore/' + restoreSnapshot, { items: items, note: document.getElementById('restoreNote').value.trim() })
				.then(function (d) { return d.result; }).then(function (r) { if (r && r.success) openRestoreSnapshot(restoreSnapshot); }).catch(function () {});
		});
	}

	wireMissions();
	wireProgress();
	wireRestore();
})();
