// API keys on the account page: list, make (with a permission picker of the maker's own permissions), rotate, revoke
(function () {
	var card = document.getElementById('apiKeys');
	if (!card) return;
	var accountId = card.dataset.account;
	var isSelf = card.dataset.self === '1';
	var canCreate = card.dataset.canCreate === '1';
	var $ = function (id) { return document.getElementById(id); };
	var catalog = null;
	var titles = {};

	var STATUS = {
		active: ['Active', 'success'],
		revoked: ['Revoked', 'secondary'],
		expired: ['Expired', 'secondary'],
		signed_out: ['Stopped by sign out', 'warning']
	};

	function loadCatalog() {
		if (catalog) return Promise.resolve(catalog);
		return api.get('/api/api_keys/permissions').then(function (d) {
			if (!d.success) throw new Error(d.error || 'Could not load permissions');
			catalog = d;
			d.permissions.forEach(function (p) { titles[p.key] = p.title; });
			return d;
		});
	}

	function scopeCell(k) {
		var html = k.allPermissions ? fmt.badge('All of ' + (isSelf ? 'your' : 'their') + ' permissions', 'primary')
			: '<span title="' + esc(k.permissions.map(function (p) { return titles[p] || p; }).join(', ')) + '">' + esc(k.permissions.length) + ' permission' + (k.permissions.length === 1 ? '' : 's') + '</span>';
		if (k.readOnly) html += ' ' + fmt.badge('read-only', 'info');
		if (k.lostPermissions.length) {
			html += '<div class="small text-warning" title="The owner can\'t do these any more, so the key can\'t either">Not working: ' + esc(k.lostPermissions.join(', ')) + '</div>';
		}
		if (k.allowedIps) html += '<div class="small text-body-secondary">From ' + esc(k.allowedIps) + '</div>';
		if (k.allowedPaths) html += '<div class="small text-body-secondary">Paths ' + esc(k.allowedPaths) + '</div>';
		return html;
	}

	function limitsCell(k) {
		var html = esc(k.effectiveRateLimit) + '/min' + (k.rateLimit ? '' : ' <span class="small text-body-secondary">(default)</span>');
		if (k.dailyQuota) html += '<div class="small">' + esc(k.todayCount) + ' of ' + esc(k.dailyQuota) + ' today</div>';
		return html;
	}

	function row(k) {
		var status = STATUS[k.status] || [k.status, 'secondary'];
		var active = k.status === 'active' || k.status === 'signed_out';
		var actions = '';
		if (isSelf && k.status !== 'revoked' && k.status !== 'expired' && canCreate) actions += '<button class="btn btn-sm btn-outline-secondary me-1" data-rotate="' + esc(k.id) + '">Rotate</button>';
		if (k.status !== 'revoked') actions += '<button class="btn btn-sm btn-outline-danger" data-revoke="' + esc(k.id) + '">Revoke</button>';
		return '<tr' + (active ? '' : ' class="text-body-secondary"') + '>' +
			'<td><strong>' + esc(k.name) + '</strong> <code class="small">' + esc(k.prefix) + '…</code>' +
				(k.note ? '<div class="small text-body-secondary">' + esc(k.note) + '</div>' : '') +
				'<div class="small text-body-secondary">Made ' + fmt.unix(k.createdAt) + (k.createdBy ? ' by ' + esc(k.createdBy) : '') + '</div></td>' +
			'<td>' + scopeCell(k) + '</td>' +
			'<td>' + limitsCell(k) + '</td>' +
			'<td>' + (k.lastUsedAt ? fmt.unix(k.lastUsedAt) + '<div class="small text-body-secondary">' + esc(k.lastIp) + '</div>' : '<span class="text-body-secondary">Never</span>') + '</td>' +
			'<td>' + esc(k.requestCount) + '</td>' +
			'<td>' + (k.expiresAt ? fmt.unix(k.expiresAt) : 'Never') + '</td>' +
			'<td>' + fmt.badge(status[0], status[1]) + (k.revokedAt ? '<div class="small text-body-secondary">' + fmt.unix(k.revokedAt) + (k.revokedBy ? ' by ' + esc(k.revokedBy) : '') + '</div>' : '') + '</td>' +
			'<td class="text-nowrap text-end">' + actions + '</td></tr>';
	}

	function load() {
		return loadCatalog().catch(function () { return null; }).then(function () {
			return api.get('/api/accounts/' + accountId + '/api_keys');
		}).then(function (d) {
			var rows = $('apiKeyRows');
			if (!d.success) { rows.innerHTML = '<tr><td colspan="8" class="text-danger">' + esc(d.error || 'Could not load the keys') + '</td></tr>'; return; }
			rows.innerHTML = d.keys.length ? d.keys.map(row).join('') : '<tr><td colspan="8" class="text-body-secondary">No API keys.</td></tr>';
		});
	}

	// The maker's own permissions, grouped by category like the Permissions page
	function renderPicker() {
		var groups = {};
		var order = [];
		catalog.permissions.forEach(function (p) {
			if (!p.allowed) return;
			if (!groups[p.category]) { groups[p.category] = []; order.push(p.category); }
			groups[p.category].push(p);
		});
		$('apiKeyPicker').innerHTML = order.map(function (category, i) {
			return '<fieldset class="mb-2"><legend class="fs-6 fw-semibold mb-1">' + esc(category) +
				' <button type="button" class="btn btn-link btn-sm p-0 ms-1 align-baseline" data-group="' + i + '">all</button></legend><div class="row g-1">' +
				groups[category].map(function (p) {
					var id = 'apiKeyPerm_' + p.key;
					return '<div class="col-md-6 col-xl-4"><div class="form-check" title="' + esc(p.description) + '">' +
						'<input class="form-check-input" type="checkbox" value="' + esc(p.key) + '" id="' + id + '" data-group-of="' + i + '">' +
						'<label class="form-check-label" for="' + id + '">' + esc(p.title) + ' <span class="text-body-secondary">(' + esc(p.key) + ')</span></label></div></div>';
				}).join('') + '</div></fieldset>';
		}).join('') || '<p class="text-body-secondary">You have no permissions to give a key.</p>';
		$('apiKeyRate').placeholder = 'Default (' + catalog.defaultRateLimit + ')';
		$('apiKeyRate').max = catalog.maxRateLimit;
	}

	function showForm(show) {
		$('apiKeyForm').classList.toggle('d-none', !show);
		if (show) loadCatalog().then(renderPicker).catch(function (e) { toast(e.message, 'danger'); });
	}

	function showSecret(key) {
		$('apiKeySecret').value = key;
		$('apiKeyCreated').classList.remove('d-none');
		$('apiKeySecret').select();
	}

	if ($('apiKeyNew')) $('apiKeyNew').addEventListener('click', function () { showForm(true); });
	$('apiKeyCancel').addEventListener('click', function () { showForm(false); });
	$('apiKeyCopy').addEventListener('click', function () {
		var box = $('apiKeySecret');
		box.select();
		if (navigator.clipboard) navigator.clipboard.writeText(box.value).then(function () { toast('Copied', 'success'); });
	});
	$('apiKeyScopeAll').addEventListener('change', function () { $('apiKeyPicker').classList.add('d-none'); });
	$('apiKeyScopeSome').addEventListener('change', function () { $('apiKeyPicker').classList.remove('d-none'); });
	$('apiKeyPicker').addEventListener('click', function (e) {
		var group = e.target.dataset.group;
		if (group === undefined) return;
		var boxes = $('apiKeyPicker').querySelectorAll('[data-group-of="' + group + '"]');
		var check = Array.prototype.some.call(boxes, function (b) { return !b.checked; });
		boxes.forEach(function (b) { b.checked = check; });
	});

	$('apiKeyForm').addEventListener('submit', function (e) {
		e.preventDefault();
		var all = $('apiKeyScopeAll').checked;
		var picked = Array.prototype.map.call($('apiKeyPicker').querySelectorAll('input:checked'), function (b) { return b.value; });
		if (!all && !picked.length) { toast('Pick at least one permission, or all of yours', 'warning'); return; }
		api.action('/api/api_keys', {
			name: $('apiKeyName').value,
			note: $('apiKeyNote').value,
			permissions: all ? '*' : picked,
			readOnly: $('apiKeyReadOnly').checked,
			allowedIps: $('apiKeyIps').value,
			allowedPaths: $('apiKeyPaths').value,
			rateLimit: parseInt($('apiKeyRate').value, 10) || 0,
			dailyQuota: parseInt($('apiKeyQuota').value, 10) || 0,
			expiresInDays: parseInt($('apiKeyExpiry').value, 10) || 0
		}).then(function (d) {
			$('apiKeyForm').reset();
			$('apiKeyPicker').classList.remove('d-none');
			showForm(false);
			showSecret(d.key);
			load();
		}).catch(function () {});
	});

	$('apiKeyRows').addEventListener('click', function (e) {
		var rotate = e.target.dataset.rotate, revoke = e.target.dataset.revoke;
		if (rotate) {
			if (!confirm('Make a new secret for this key? The current one stops working at once.')) return;
			api.action('/api/api_keys/' + rotate + '/rotate', {}).then(function (d) { showSecret(d.key); load(); }).catch(function () {});
		} else if (revoke) {
			if (!confirm('Revoke this key? Anything using it stops working at once.')) return;
			api.action('/api/api_keys/' + revoke + '/revoke', {}, 'API key revoked').then(load).catch(function () {});
		}
	});

	load();
})();
