/**
 * Character page: the editor (coins, U-score, level, items) and the history (snapshots compared with the character
 * now, download, restore). Saving goes through the server's safe path: the owner is disconnected first and the
 * current version is kept as a snapshot.
 */
(function () {
	'use strict';
	var config = window.characterToolsConfig;
	if (!config) return;
	var nf = new Intl.NumberFormat();
	var current = null;
	var edit = null; // { counts: {id: n}, remove: {id: true}, add: [{lot, name, count, inventory}] }
	var inventoryTab = 0;

	function inventoryName(type) { return Labels.name('inventories', type) || 'Inventory ' + type; }
	function itemLabel(item) { return esc(item.name || 'LOT ' + item.lot) + ' <span class="text-body-secondary small">' + esc(item.lot) + '</span>'; }

	function loadSummary() {
		return api.get('/api/characters/' + config.id + '/summary').then(function (d) {
			if (!d.success) { toast(d.error || 'Could not read the character', 'danger'); return null; }
			current = d.summary;
			return current;
		});
	}

	// ---- editor ----
	function renderEditItems() {
		var inventory = current.inventories.filter(function (i) { return i.type === inventoryTab; })[0] || { items: [], size: 0 };
		document.getElementById('editInventoryTabs').innerHTML = current.inventories.filter(function (i) { return i.items.length || i.type === 0; }).map(function (i) {
			return '<li class="nav-item"><button type="button" class="nav-link' + (i.type === inventoryTab ? ' active' : '') + '" data-inventory="' + i.type + '">' +
				esc(inventoryName(i.type)) + ' <span class="badge text-bg-secondary">' + i.items.length + (i.size ? '/' + i.size : '') + '</span></button></li>';
		}).join('');
		var rows = inventory.items.slice().sort(function (a, b) { return a.slot - b.slot; }).map(function (item) {
			var removed = !!edit.remove[item.id];
			var count = edit.counts[item.id] !== undefined ? edit.counts[item.id] : item.count;
			return '<tr' + (removed ? ' class="text-decoration-line-through opacity-50"' : '') + '><td data-label="Item">' + itemLabel(item) + (item.equipped ? ' ' + fmt.badge('equipped', 'info') : '') + '</td>' +
				'<td data-label="Slot">' + esc(item.slot) + '</td>' +
				'<td data-label="Count"><input type="number" min="1" max="999999" class="form-control form-control-sm" data-count="' + esc(item.id) + '" value="' + esc(count) + '"' + (removed ? ' disabled' : '') + '></td>' +
				'<td class="text-end"><input class="form-check-input" type="checkbox" data-remove="' + esc(item.id) + '"' + (removed ? ' checked' : '') + ' aria-label="Remove ' + esc(item.name) + '"></td></tr>';
		});
		document.getElementById('editItems').innerHTML = rows.join('') || '<tr><td colspan="4" class="text-body-secondary">Empty</td></tr>';
		document.getElementById('pendingAdds').innerHTML = edit.add.map(function (a, i) {
			return '<li>Add ' + esc(a.count) + 'x ' + esc(a.name) + ' to ' + esc(inventoryName(a.inventory)) + ' <a href="#" data-undo-add="' + i + '">undo</a></li>';
		}).join('');
		describeChanges();
	}

	function buildEdit() {
		var body = {};
		var coins = document.getElementById('editCoins').value, uscore = document.getElementById('editUScore').value, level = document.getElementById('editLevel').value;
		if (coins !== '' && Number(coins) !== current.coins) body.coins = Number(coins);
		if (uscore !== '' && Number(uscore) !== current.uscore) body.uscore = Number(uscore);
		if (level !== '' && Number(level) !== current.level) body.level = Number(level);
		var counts = {};
		Object.keys(edit.counts).forEach(function (id) { if (!edit.remove[id]) counts[id] = edit.counts[id]; });
		if (Object.keys(counts).length) body.counts = counts;
		var remove = Object.keys(edit.remove).filter(function (id) { return edit.remove[id]; });
		if (remove.length) body.remove = remove;
		if (edit.add.length) body.add = edit.add.map(function (a) { return { lot: a.lot, count: a.count, inventory: a.inventory }; });
		return body;
	}

	function describeChanges() {
		var body = buildEdit(), parts = [];
		['coins', 'uscore', 'level'].forEach(function (k) { if (body[k] !== undefined) parts.push(k === 'uscore' ? 'U-score' : k); });
		if (body.counts) parts.push(Object.keys(body.counts).length + ' count(s)');
		if (body.remove) parts.push(body.remove.length + ' removed');
		if (body.add) parts.push(body.add.length + ' added');
		document.getElementById('editChanges').textContent = parts.length ? 'Changes: ' + parts.join(', ') : 'No changes yet';
		document.getElementById('saveEditBtn').disabled = !parts.length;
	}

	function openEditor() {
		loadSummary().then(function (summary) {
			if (!summary) return;
			edit = { counts: {}, remove: {}, add: [] };
			inventoryTab = 0;
			document.getElementById('editCoins').value = summary.coins;
			document.getElementById('editUScore').value = summary.uscore;
			document.getElementById('editLevel').value = summary.level;
			renderEditItems();
			bootstrap.Modal.getOrCreateInstance(document.getElementById('editModal')).show();
		});
	}

	var suggestions = {};
	var searchTimer = null;
	function wireEditor() {
		var button = document.getElementById('editCharacterBtn');
		if (!button) return;
		button.addEventListener('click', openEditor);
		['editCoins', 'editUScore', 'editLevel'].forEach(function (id) { document.getElementById(id).addEventListener('input', describeChanges); });
		document.getElementById('editInventoryTabs').addEventListener('click', function (e) {
			var tab = e.target.closest('[data-inventory]');
			if (tab) { inventoryTab = Number(tab.dataset.inventory); renderEditItems(); }
		});
		document.getElementById('editItems').addEventListener('input', function (e) {
			if (e.target.dataset.count) { edit.counts[e.target.dataset.count] = Number(e.target.value); describeChanges(); }
		});
		document.getElementById('editItems').addEventListener('change', function (e) {
			if (e.target.dataset.remove) { edit.remove[e.target.dataset.remove] = e.target.checked; renderEditItems(); }
		});
		document.getElementById('pendingAdds').addEventListener('click', function (e) {
			var undo = e.target.closest('[data-undo-add]');
			if (undo) { e.preventDefault(); edit.add.splice(Number(undo.dataset.undoAdd), 1); renderEditItems(); }
		});
		var input = document.getElementById('addItemName');
		input.addEventListener('input', function () {
			clearTimeout(searchTimer);
			var q = input.value.trim();
			if (q.length < 2 || /^\d+$/.test(q)) return;
			searchTimer = setTimeout(function () {
				api.get('/api/items/search?q=' + encodeURIComponent(q)).then(function (items) {
					suggestions = {};
					document.getElementById('addItemList').innerHTML = (Array.isArray(items) ? items : []).map(function (i) {
						suggestions[i.name] = i.lot;
						return '<option value="' + esc(i.name) + '">' + esc(i.lot) + '</option>';
					}).join('');
				});
			}, 250);
		});
		document.getElementById('addItemBtn').addEventListener('click', function () {
			var text = input.value.trim();
			var lot = /^\d+$/.test(text) ? Number(text) : suggestions[text];
			var count = Number(document.getElementById('addItemCount').value) || 1;
			if (!lot) { toast('Pick an item from the list or type its LOT', 'warning'); return; }
			edit.add.push({ lot: lot, name: /^\d+$/.test(text) ? 'LOT ' + lot : text, count: count, inventory: inventoryTab });
			input.value = '';
			renderEditItems();
		});
		document.getElementById('saveEditBtn').addEventListener('click', function () {
			var body = buildEdit();
			if (!confirm('Save these changes? The player is disconnected if online.')) return;
			api.action('/api/characters/' + config.id + '/edit', body).then(function (d) { return d.result; }).then(function (r) {
				if (r && r.success) { bootstrap.Modal.getInstance(document.getElementById('editModal')).hide(); setTimeout(function () { location.reload(); }, 600); }
			}).catch(function () {});
		});
	}

	// ---- history ----
	function itemsById(summary) {
		var map = {};
		summary.inventories.forEach(function (inventory) {
			inventory.items.forEach(function (item) { map[item.id] = { item: item, inventory: inventory.type }; });
		});
		return map;
	}

	function compare(before, after) {
		var rows = [];
		[['coins', 'Coins'], ['uscore', 'U-score'], ['level', 'Level'], ['missions_done', 'Missions done']].forEach(function (f) {
			if (before[f[0]] !== after[f[0]]) rows.push('<tr><td>' + f[1] + '</td><td>' + nf.format(before[f[0]]) + '</td><td>' + nf.format(after[f[0]]) + '</td></tr>');
		});
		var a = itemsById(before), b = itemsById(after);
		Object.keys(a).forEach(function (id) {
			var old = a[id], now = b[id];
			if (!now) rows.push('<tr class="table-danger"><td>' + itemLabel(old.item) + ' <span class="small">(' + esc(inventoryName(old.inventory)) + ')</span></td><td>' + nf.format(old.item.count) + '</td><td>gone</td></tr>');
			else if (now.item.count !== old.item.count || now.inventory !== old.inventory) {
				rows.push('<tr class="table-warning"><td>' + itemLabel(old.item) + '</td><td>' + nf.format(old.item.count) + ' in ' + esc(inventoryName(old.inventory)) + '</td><td>' + nf.format(now.item.count) + ' in ' + esc(inventoryName(now.inventory)) + '</td></tr>');
			}
		});
		Object.keys(b).forEach(function (id) {
			if (!a[id]) rows.push('<tr class="table-success"><td>' + itemLabel(b[id].item) + ' <span class="small">(' + esc(inventoryName(b[id].inventory)) + ')</span></td><td>not there</td><td>' + nf.format(b[id].item.count) + '</td></tr>');
		});
		return rows;
	}

	function openSnapshot(id) {
		document.querySelectorAll('#snapshotList [data-snapshot]').forEach(function (el) { el.classList.toggle('active', el.dataset.snapshot === String(id)); });
		var box = document.getElementById('snapshotCompare');
		box.innerHTML = '<p class="text-body-secondary">Loading&hellip;</p>';
		Promise.all([api.get('/api/snapshots/' + id), loadSummary()]).then(function (res) {
			var snap = res[0];
			if (!snap.success || !current) { box.innerHTML = '<p class="text-danger">' + esc(snap.error || 'Could not load it') + '</p>'; return; }
			var rows = compare(snap.summary, current);
			box.innerHTML = '<div class="d-flex flex-wrap gap-2 align-items-center mb-2"><strong class="me-auto">' + esc(fmt.unix(snap.taken_at)) + ' &rarr; now</strong>' +
				'<a class="btn btn-sm btn-outline-secondary" href="/api/snapshots/' + esc(id) + '/xml">Download XML</a>' +
				(config.canEdit ? '<button class="btn btn-sm btn-danger" data-restore="' + esc(id) + '">Restore this version</button>' : '') + '</div>' +
				(rows.length ? '<div class="table-responsive"><table class="table table-sm align-middle"><thead><tr><th>What</th><th>Then</th><th>Now</th></tr></thead><tbody>' + rows.join('') + '</tbody></table></div>'
					: '<p class="text-body-secondary">No differences in coins, level or items.</p>');
		});
	}

	function openHistory() {
		bootstrap.Modal.getOrCreateInstance(document.getElementById('historyModal')).show();
		api.get('/api/characters/' + config.id + '/snapshots').then(function (d) {
			var list = document.getElementById('snapshotList');
			if (!d.success) { list.innerHTML = '<div class="text-danger">' + esc(d.error) + '</div>'; return; }
			list.innerHTML = d.snapshots.map(function (s) {
				return '<a href="#" class="list-group-item list-group-item-action" data-snapshot="' + s.id + '"><div class="fw-semibold">' + esc(fmt.unix(s.taken_at)) + '</div>' +
					'<div class="text-body-secondary">' + esc(s.reason) + (s.actor ? ' by ' + esc(s.actor) : '') + '</div></a>';
			}).join('') || '<div class="text-body-secondary">No snapshots yet. One is kept before every change made here, and the Character snapshots task saves changed characters daily.</div>';
		});
	}

	function wireHistory() {
		var button = document.getElementById('historyBtn');
		if (!button) return;
		button.addEventListener('click', openHistory);
		document.getElementById('snapshotList').addEventListener('click', function (e) {
			var item = e.target.closest('[data-snapshot]');
			if (item) { e.preventDefault(); openSnapshot(item.dataset.snapshot); }
		});
		document.getElementById('snapshotCompare').addEventListener('click', function (e) {
			var restore = e.target.closest('[data-restore]');
			if (!restore || !confirm('Put the character back to this version? The player is disconnected if online, and the current version is kept in the history.')) return;
			api.action('/api/snapshots/' + restore.dataset.restore + '/restore', {}).then(function (d) { return d.result; }).then(function (r) {
				if (r && r.success) setTimeout(function () { location.reload(); }, 600);
			}).catch(function () {});
		});
	}

	wireEditor();
	wireHistory();
})();
