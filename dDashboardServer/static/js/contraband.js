/**
 * The Contraband page: the list of items players shouldn't have, and (with contraband_manage) adding, changing and
 * removing entries. Running worlds reload the list after every change.
 */
(function () {
	'use strict';

	var canManage = false;
	var lotInput = document.getElementById('lotInput');

	function row(i) {
		var action = i.action === 'remove' ? fmt.badge('Flag and remove', 'danger') : fmt.badge('Flag', 'warning');
		var buttons = canManage ? '<button type="button" class="btn btn-sm btn-outline-secondary me-1" data-edit="' + esc(i.lot) + '">Edit</button>' +
			'<button type="button" class="btn btn-sm btn-outline-danger" data-remove="' + esc(i.lot) + '">Remove</button>' : '';
		return '<tr><td><strong>' + esc(i.name) + '</strong> <span class="text-body-secondary small">' + esc(i.lot) + '</span></td>' +
			'<td>' + esc(i.reason) + '</td><td>' + action + '</td><td class="small">' + esc(i.added_by) + ', ' + esc(fmt.unix(i.added_at)) + '</td>' +
			'<td class="text-end text-nowrap">' + buttons + '</td></tr>';
	}

	var items = [];
	function load() {
		api.get('/api/contraband').then(function (d) {
			if (!d.success) return;
			items = d.items;
			canManage = d.canManage;
			document.getElementById('addCard').classList.toggle('d-none', !canManage);
			document.getElementById('rows').innerHTML = items.map(row).join('') ||
				'<tr><td colspan="5" class="text-body-secondary">Nothing is contraband.</td></tr>';
		}).catch(function () {});
	}

	var timer = null;
	lotInput.addEventListener('input', function () {
		clearTimeout(timer);
		var q = lotInput.value.trim();
		if (q.length < 2 || /^\d+$/.test(q)) return;
		timer = setTimeout(function () {
			api.get('/api/contraband/items?q=' + encodeURIComponent(q)).then(function (found) {
				document.getElementById('lotSuggestions').innerHTML = (Array.isArray(found) ? found : []).map(function (i) {
					return '<option value="' + esc(i.lot) + '">' + esc(i.name) + '</option>';
				}).join('');
			});
		}, 250);
	});

	function done(d) {
		if (!d.success) { toast(d.error || 'Failed', 'danger'); return; }
		toast(d.message, 'success');
		load();
	}

	document.getElementById('addForm').addEventListener('submit', function (e) {
		e.preventDefault();
		var lot = parseInt(lotInput.value, 10);
		if (!(lot > 0)) { toast('Pick an item from the list or enter its LOT', 'warning'); return; }
		api.post('/api/contraband', { lot: lot, reason: document.getElementById('reasonInput').value, action: document.getElementById('actionInput').value })
			.then(function (d) { if (d.success) { lotInput.value = ''; document.getElementById('reasonInput').value = ''; } done(d); });
	});

	document.getElementById('rows').addEventListener('click', function (e) {
		var edit = e.target.closest('[data-edit]'), remove = e.target.closest('[data-remove]');
		if (edit) {
			var item = items.filter(function (i) { return String(i.lot) === edit.dataset.edit; })[0];
			if (!item) return;
			lotInput.value = item.lot;
			document.getElementById('reasonInput').value = item.reason;
			document.getElementById('actionInput').value = item.action;
			lotInput.focus();
		} else if (remove) {
			if (!confirm('Take this item off the contraband list?')) return;
			api.post('/api/contraband/delete', { lot: parseInt(remove.dataset.remove, 10) }).then(done);
		}
	});

	load();
})();
