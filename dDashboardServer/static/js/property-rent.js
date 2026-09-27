/**
 * The Property Rent page: each property world's rent from its template, and (with property_rent_manage) a price and
 * period to charge instead.
 */
(function () {
	'use strict';

	var canManage = false;

	function rate(r) {
		if (!r) return '<span class="text-body-secondary">Free</span>';
		return esc(r.price) + ' coins / ' + esc(r.periodDays) + ' days';
	}

	function row(w) {
		var o = w.override;
		var set = o ? esc(o.price) + ' coins' + (o.periodDays ? ' / ' + esc(o.periodDays) + ' days' : '') +
			'<div class="small text-body-secondary">' + esc(o.updated_by) + ', ' + esc(fmt.unix(o.updated_at)) + '</div>' : '<span class="text-body-secondary">Template</span>';
		var edit = canManage ? '<form class="d-flex gap-1 justify-content-end" data-map="' + esc(w.mapId) + '">' +
			'<input type="number" class="form-control form-control-sm" style="width:7rem" min="0" name="price" placeholder="Coins" value="' + (o ? esc(o.price) : '') + '" aria-label="Price">' +
			'<input type="number" class="form-control form-control-sm" style="width:6rem" min="0" name="days" placeholder="Days" value="' + (o && o.periodDays ? esc(o.periodDays) : '') + '" aria-label="Period in days">' +
			'<button type="submit" class="btn btn-sm btn-primary">Save</button>' +
			(o ? '<button type="button" class="btn btn-sm btn-outline-secondary" data-reset="' + esc(w.mapId) + '">Template</button>' : '') + '</form>' : '';
		return '<tr><td>' + esc(w.name) + ' <span class="text-body-secondary small">' + esc(w.mapId) + '</span></td><td>' + rate(w.template) + '</td><td>' + set +
			'</td><td>' + rate(w.rent) + '</td><td>' + edit + '</td></tr>';
	}

	function load() {
		api.get('/api/property_rent').then(function (d) {
			if (!d.success) return;
			canManage = d.canManage;
			document.getElementById('offNote').classList.toggle('d-none', d.enabled);
			document.getElementById('rows').innerHTML = d.worlds.map(row).join('') || '<tr><td colspan="5" class="text-body-secondary">No property worlds found in the CDClient.</td></tr>';
		}).catch(function () {});
	}

	function done(d) {
		if (!d.success) { toast(d.error || 'Failed', 'danger'); return; }
		toast(d.message, 'success');
		load();
	}

	var rows = document.getElementById('rows');
	rows.addEventListener('submit', function (e) {
		e.preventDefault();
		var form = e.target;
		var price = parseInt(form.price.value, 10), days = parseInt(form.days.value || '0', 10);
		if (!(price >= 0)) { toast('Enter a price in coins (0: free)', 'warning'); return; }
		api.post('/api/property_rent', { mapId: parseInt(form.dataset.map, 10), price: price, periodDays: days >= 0 ? days : 0 }).then(done);
	});
	rows.addEventListener('click', function (e) {
		var reset = e.target.closest('[data-reset]');
		if (reset) api.post('/api/property_rent/delete', { mapId: parseInt(reset.dataset.reset, 10) }).then(done);
	});

	load();
})();
