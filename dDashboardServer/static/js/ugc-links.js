/**
 * Players' creations on the property and character pages: the icon the UGC server made of a model, car or rocket in
 * place of the item's own icon, its state and a link to it on the /ugc page (for those who may open it). The routes
 * (/api/ugc_links/...) let whoever may view the page see the icons of the creations on it.
 */
(function () {
	var STATES = { done: ['Made', 'success'], pending: ['Waiting', 'secondary'], failed: ['Failed', 'danger'] };

	function row(label, value) { return value === '' || value === null || value === undefined ? '' : '<dt class="col-5">' + esc(label) + '</dt><dd class="col-7 text-break">' + value + '</dd>'; }

	// Triangles before and after hidden surfaces were removed, per LOD
	function triangles(stats) {
		return (stats.lods || []).map(function (l) {
			return 'LOD ' + esc(l.lod) + ': ' + esc(l.opaqueBefore) + ' &rarr; ' + esc(l.opaqueAfter) + (l.transparent ? ' + ' + esc(l.transparent) + ' transparent' : '') +
				' <span class="text-body-secondary">(' + esc(l.vertices) + ' vertices)</span>';
		}).join('<br>');
	}

	function timings(ms) {
		if (!ms) return '';
		var parts = [['build', 'build'], ['hiddenSurfaces', 'hidden faces'], ['ambientOcclusion', 'AO'], ['icon', 'icon']]
			.filter(function (p) { return ms[p[0]] !== undefined; }).map(function (p) { return esc(p[1]) + ' ' + esc(ms[p[0]]); });
		return esc(ms.total) + ' ms' + (parts.length ? ' <span class="text-body-secondary">(' + parts.join(', ') + ')</span>' : '');
	}

	function renderDetails(d, modelId) {
		var placement = (d.placements || []).find(function (p) { return p.modelId === String(modelId); }) || (d.placements || [])[0];
		var name = placement && placement.name && !/^Objects_\d+_name$/.test(placement.name) ? placement.name : '';
		var s = d.stats, prev = d.previousStats;
		var waiting = d.state === 'pending' && d.processAfter > Date.now() / 1000;
		var html = '<div class="ugc-details small"><div class="d-flex gap-2 mb-2 align-items-end">' +
			'<figure class="m-0 text-center"><img src="' + esc(d.icon) + '" width="96" height="96" class="rounded bg-body-tertiary" alt="" onerror="this.style.visibility=\'hidden\'"><figcaption class="text-body-secondary">Now</figcaption></figure>' +
			'<figure class="m-0 text-center d-none"><img src="' + esc(d.previousIcon) + '" width="64" height="64" class="rounded bg-body-tertiary" alt="" onload="this.parentNode.classList.remove(\'d-none\')"><figcaption class="text-body-secondary">Before &middot; <a href="' + esc(d.previousNif) + '">NIF</a></figcaption></figure></div>' +
			'<dl class="row mb-2">' +
			row('State', UgcLinks.state(d) + (d.attempts ? ' <span class="text-body-secondary">' + esc(d.attempts) + ' attempt' + (d.attempts === 1 ? '' : 's') + '</span>' : '') +
				(d.error ? '<div class="text-danger">' + esc(d.error) + '</div>' : '')) +
			row('Waiting until', waiting ? esc(fmt.unix(d.processAfter)) + ' <span class="text-body-secondary">(the owner saved it recently)</span>' : '') +
			row('UGC id', esc(d.ugcId)) +
			row('Owner', (d.characterName ? fmt.character(d.characterId, d.characterName) : esc(d.characterId)) + (d.accountName ? ' <span class="text-body-secondary">(' + esc(d.accountName) + ')</span>' : '')) +
			row('Name', name ? esc(name) : '') +
			row('Description', placement && placement.description ? esc(placement.description) : '') +
			row('File', d.fileName ? esc(d.fileName) : '') +
			row('Made', d.processedAt ? esc(fmt.unix(d.processedAt)) : '') +
			row('Baked AO', d.bakeAo === undefined ? '' : (d.bakeAo ? 'Yes' : 'No')) +
			row('Bricks', s ? esc(s.bricks) + (prev && prev.bricks !== s.bricks ? ' <span class="text-body-secondary">(was ' + esc(prev.bricks) + ')</span>' : '') : '') +
			row('Triangles', s ? triangles(s) : '') +
			row('Time', s ? timings(s.ms) : '') +
			row('Before', prev ? esc(prev.bricks) + ' bricks<br>' + triangles(prev) : '') +
			'</dl>';
		if (d.state === 'done' && !s) html += '<div class="text-body-secondary mb-2">No stats (made before the UGC server wrote them).</div>';
		var buttons = '';
		if (d.link) buttons += '<a class="btn btn-sm btn-outline-primary" href="' + esc(d.link) + '">Open in UGC viewer</a>';
		if (d.canManage) buttons += '<button type="button" class="btn btn-sm btn-outline-warning" data-ugc-remake="' + esc(d.ugcId) + '">Make again</button>';
		if (d.state === 'done') buttons += '<a class="btn btn-sm btn-outline-secondary" href="' + esc(d.nif) + '">Download NIF</a>';
		if (modelId) buttons += '<a class="btn btn-sm btn-outline-secondary" href="/api/property_models/' + esc(modelId) + '/lxfml">Download LXFML</a>';
		return html + (buttons ? '<div class="d-flex flex-wrap gap-1">' + buttons + '</div>' : '') + '</div>';
	}

	// "Make again" in any UGC block
	document.addEventListener('click', function (e) {
		var button = e.target.closest && e.target.closest('[data-ugc-remake]');
		if (!button) return;
		button.disabled = true;
		api.post('/api/ugc/reprocess', { kind: 'model', id: button.dataset.ugcRemake }).then(function (d) {
			toast(d.success ? 'The UGC server will make it again' : (d.error || 'Failed'), d.success ? 'success' : 'danger');
		}).catch(function () { toast('Failed', 'danger'); }).then(function () { button.disabled = false; });
	});

	// Expandable blocks (<details data-ugc-details="ugcId" data-model="modelId" data-via="property=...">) load when first opened
	document.addEventListener('toggle', function (e) {
		var box = e.target;
		if (!box.matches || !box.matches('details[data-ugc-details]') || !box.open || box.dataset.loaded) return;
		box.dataset.loaded = '1';
		var body = box.querySelector('.ugc-details-body');
		UgcLinks.details(box.dataset.ugcDetails, box.dataset.via, box.dataset.model).then(function (html) { body.innerHTML = html; });
	}, true);

	var linkCache = {}; // character id -> promise of its item links

	window.UgcLinks = {
		// A badge for the UGC server's state; a failure's reason as its tooltip
		state: function (item) {
			var s = STATES[item.state] || [item.state, 'secondary'];
			return '<span class="badge text-bg-' + s[1] + '"' + (item.error ? ' title="' + esc(item.error) + '"' : '') + '>' + esc(s[0]) + '</span>';
		},
		// An <img> of the creation's icon that falls back to the item's own icon until the UGC server made one
		icon: function (item, lot, size) {
			size = size || 40;
			return '<img src="' + esc(item.icon) + '" width="' + size + '" height="' + size + '" class="rounded bg-body-tertiary" alt="" loading="lazy" ' +
				'onerror="this.onerror=null;this.src=\'/api/icon/' + esc(lot) + '\'">';
		},
		/**
		 * The UGC block for a brick-built model on the property pages: its icons (and the previous version's), state,
		 * owner, the name the player gave it, the UGC server's stats and links. `via` is the page's ?property= / ?character=
		 * query that lets its viewer see it; `modelId` (a placed model) picks its name and LXFML download.
		 */
		details: function (ugcId, via, modelId) {
			if (!ugcId || ugcId === '0') return Promise.resolve('<div class="small text-body-secondary">No UGC data: this model has no blueprint.</div>');
			return api.get('/api/ugc_links/model/' + encodeURIComponent(ugcId) + (via ? '?' + via : '')).then(function (d) {
				if (!d || d.success === false) return '<div class="small text-body-secondary">' + esc((d && d.error) || 'No UGC data for this model') + '</div>';
				return renderDetails(d, modelId);
			}).catch(function (e) { return '<div class="small text-body-secondary">' + esc((e && e.message) || 'Could not load the UGC data') + '</div>'; });
		},
		// {modelId: item} for the player-built models on a property		// {modelId: item} for the player-built models on a property
		forProperty: function (propertyId) {
			return api.get('/api/ugc_links/property/' + propertyId).then(function (d) {
				var map = {};
				((d && d.items) || []).forEach(function (item) { map[item.modelId] = item; });
				return map;
			}).catch(function () { return {}; });
		},
		// Shows the creations among a character page's items (.inv-item[data-id]) with their own icon and a link
		// Itemid -> the UGC link of each of a character's player-built items (fetched once per character)
		inventoryLinks: function (characterId) {
			if (!linkCache[characterId]) {
				linkCache[characterId] = api.get('/api/ugc_links/character/' + characterId).then(function (d) {
					var map = {};
					((d && d.items) || []).forEach(function (item) { map[item.itemId] = item; });
					return map;
				});
			}
			return linkCache[characterId];
		},

		// Shows a player-built item's UGC icon and a link to the UGC viewer on one inventory item element
		decorateItem: function (el, item) {
			var img = el.querySelector('img');
			if (img) {
				var own = img.src;
				img.onerror = function () { img.onerror = null; img.src = own; };
				img.src = item.icon;
			}
			el.classList.add('inv-ugc');
			el.title = (el.title ? el.title + ' - ' : '') + 'Player-built ' + (item.kind === 'modular' ? 'car or rocket' : 'model') + ' (UGC ' + item.ugcId + ', ' + item.state + ')';
			if (item.link && !el.querySelector('.inv-ugc-link')) {
				// Items may sit inside a link already (the item trace), so this is a button, not a nested link
				var a = document.createElement('span');
				a.className = 'inv-ugc-link';
				a.setAttribute('role', 'link');
				a.tabIndex = 0;
				a.textContent = 'UGC';
				a.title = 'Open in the UGC viewer';
				var open = function (e) { e.preventDefault(); e.stopPropagation(); window.location.href = item.link; };
				a.addEventListener('click', open);
				a.addEventListener('keydown', function (e) { if (e.key === 'Enter') open(e); });
				el.appendChild(a);
			}
		},

		// Decorates the inventory items already on the page
		decorateInventory: function (characterId, root) {
			var self = this;
			return this.inventoryLinks(characterId).then(function (map) {
				Object.keys(map).forEach(function (id) {
					(root || document).querySelectorAll('.inv-item[data-id="' + id + '"]').forEach(function (el) { self.decorateItem(el, map[id]); });
				});
			}).catch(function () {});
		}
	};
})();
