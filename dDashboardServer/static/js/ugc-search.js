/**
 * The UGC search (/ugc_search): players' creations by name, id or LOT and where each one is (/api/ugc_links/search).
 * ?q= fills the search in; the address keeps the last search so it can be shared.
 */
(function () {
	var input = document.getElementById('ugcSearchInput');
	var rows = document.getElementById('ugcSearchRows');
	var status = document.getElementById('ugcSearchStatus');
	var latest = 0, timer = null;
	var INVENTORIES = { 5: 'Models', 12: 'Vault models', 14: 'Brick-building models' };

	function inventoryName(type) { return (window.Labels && Labels.name('inventories', type)) || INVENTORIES[type] || 'Inventory ' + type; }

	function where(item) {
		if (!item.where.length) {
			return '<span class="text-body-secondary small" title="Not placed on a property, not in the mail and not in its creator\'s inventories: traded, sold or deleted">Not found</span>';
		}
		return item.where.map(function (w) {
			if (w.type === 'property') {
				// A model the player didn't name has the client's placeholder (Objects_<lot>_name)
				var given = w.modelName && !/^Objects_\d+_name$/.test(w.modelName);
				var name = given ? ' as <span class="text-break">' + esc(w.modelName) + '</span>' : '';
				return '<div>On ' + fmt.property(w.propertyId, w.propertyName || 'Property ' + w.propertyId) + name +
					' <span class="small text-body-secondary">(' + fmt.character(w.ownerId, w.ownerName) + ', ' + esc(w.zoneId) + ')</span>' +
					' <a class="small" href="/properties/' + esc(w.propertyId) + '/3d#model=' + esc(w.modelId) + '">3D</a></div>';
			}
			if (w.type === 'mail') return '<div>In the mail to ' + fmt.character(w.characterId, w.characterName) + '</div>';
			return '<div>In ' + fmt.character(w.characterId, item.characterName) + '\'s ' + esc(inventoryName(w.inventory)) + '</div>';
		}).join('');
	}

	function render(items) {
		rows.innerHTML = items.length ? items.map(function (item) {
			var lot = item.kind === 'modular' ? 6416 : 6662;
			var label = item.kind === 'modular' ? 'Car or rocket' : 'Model';
			var title = item.link ? '<a href="' + esc(item.link) + '">' + label + ' ' + esc(item.ugcId) + '</a>' : label + ' ' + esc(item.ugcId);
			var detail = item.detail ? '<div class="small text-body-secondary text-break">' + (item.kind === 'modular' ? 'Modules <code>' + esc(item.detail) + '</code>' : esc(item.detail)) + '</div>' : '';
			var account = item.accountName ? '<div class="small text-body-secondary">' + (document.getElementById('ugcSearchPage').dataset.canAccounts ? fmt.link('/accounts/' + item.accountId, item.accountName) : esc(item.accountName)) + '</div>' : '';
			return '<tr><td data-label="">' + UgcLinks.icon(item, lot, 48) + '</td><td data-label="Creation">' + title + detail + '</td>' +
				'<td data-label="Made by">' + fmt.character(item.characterId, item.characterName) + account + '</td>' +
				'<td data-label="Where">' + where(item) + '</td><td data-label="UGC server">' + UgcLinks.state(item) + '</td></tr>';
		}).join('') : '<tr><td colspan="5" class="text-body-secondary">Nothing matches.</td></tr>';
	}

	function search() {
		var q = input.value.trim();
		var request = ++latest;
		var url = new URL(window.location.href);
		if (q) url.searchParams.set('q', q); else url.searchParams.delete('q');
		history.replaceState(null, '', url);
		if (!q) { rows.innerHTML = '<tr><td colspan="5" class="text-body-secondary">Type something to search.</td></tr>'; status.textContent = ''; return; }
		status.textContent = 'Searching…';
		api.get('/api/ugc_links/search?q=' + encodeURIComponent(q)).then(function (d) {
			if (request !== latest) return;
			var items = (d && d.items) || [];
			status.textContent = items.length + ' found' + (items.length >= 50 ? ' (the newest 50 of each kind; narrow the search for more)' : '');
			render(items);
		}).catch(function () { if (request === latest) status.textContent = 'The search failed.'; });
	}

	input.addEventListener('input', function () { clearTimeout(timer); timer = setTimeout(search, 300); });
	document.getElementById('ugcSearchForm').addEventListener('submit', function (e) { e.preventDefault(); clearTimeout(timer); search(); });
	input.value = document.getElementById('ugcSearchPage').dataset.query || '';
	if (input.value) search();
	input.focus();
})();
