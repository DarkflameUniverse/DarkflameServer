/**
 * Permission grants: dashboard permissions and in-game commands given to (or taken from) one account or character.
 * Grants.mount(el, {type, id}): the grants of one account or character, with a form to add one when the viewer may.
 * Grants.mount(el, null): every grant in force and the history, with a form that asks whose it is (Permissions page).
 * The server decides what may be granted (only what the viewer holds, on accounts they may manage); the form offers
 * only those.
 */
(function () {
	var KINDS = [
		['permission', 'Dashboard permission'],
		['command', 'In-game command'],
		['permission_group', 'Every permission of a category'],
		['command_group', 'Every command up to a GM level']
	];
	var catalog = null;

	function loadCatalog() {
		if (!catalog) {
			catalog = api.get('/api/grants/catalog').then(function (d) {
				if (!d.success) throw new Error(d.error || 'Could not load what can be granted');
				return d;
			});
			catalog.catch(function () { catalog = null; });
		}
		return catalog;
	}

	// What can be picked for a kind: [{value, label, detail}], only what the viewer may grant
	function choices(d, kind, text) {
		var list;
		if (kind === 'permission') {
			list = d.permissions.map(function (p) {
				return { value: p.key, label: p.title + ' (' + p.key + ')', detail: p.category + ' · GM ' + p.level + '+ · ' + p.description, ok: p.grantable, why: p.reason };
			});
		} else if (kind === 'command') {
			list = d.commands.map(function (c) {
				return { value: c.name, label: '/' + c.aliases[0] + (c.aliases.length > 1 ? ' (also /' + c.aliases.slice(1).join(', /') + ')' : ''),
					detail: 'GM ' + c.level + '+' + (c.permission ? ' · follows ' + c.permission : '') + ' · ' + c.help, ok: c.grantable, why: c.reason };
			});
		} else if (kind === 'permission_group') {
			list = d.permissionGroups.map(function (g) {
				return { value: g.name, label: g.name, detail: g.permissions.join(', '), ok: g.grantable, why: g.reason };
			});
		} else {
			list = d.commandGroups.map(function (g) {
				return { value: g.name, label: 'Every command up to GM ' + g.name, detail: '', ok: g.grantable, why: g.reason };
			});
		}
		var query = (text || '').toLowerCase();
		return list.filter(function (item) {
			return item.ok && (!query || (item.label + ' ' + item.value + ' ' + item.detail).toLowerCase().indexOf(query) !== -1);
		}).slice(0, 60);
	}

	function status(g) {
		if (g.status === 'removed') return fmt.badge('Removed', 'secondary');
		if (g.status === 'expired') return fmt.badge('Expired', 'secondary');
		return fmt.badge('In force', 'success');
	}

	function targetLink(g) {
		if (g.targetType === 'account') return 'Account ' + fmt.link('/accounts/' + g.targetId, g.targetName || g.targetId);
		return 'Character ' + fmt.character(g.targetId, g.targetName);
	}

	function rows(list, options) {
		if (!list.length) return '<tr><td colspan="6" class="text-body-secondary">' + esc(options.empty) + '</td></tr>';
		return list.map(function (g) {
			var when = 'By ' + esc(g.grantedBy || '?') + ', ' + fmt.unix(g.grantedAt) +
				(g.revokedAt ? '<br>Removed by ' + esc(g.revokedBy) + ', ' + fmt.unix(g.revokedAt) : '');
			return '<tr><td>' + (g.deny ? fmt.badge('Deny', 'danger') : fmt.badge('Grant', 'primary')) + '</td>' +
				'<td><div class="fw-semibold">' + esc(g.label) + '</div>' + (options.showTarget ? '<div class="small">' + targetLink(g) + '</div>' : '') +
				(g.note ? '<div class="small text-body-secondary">' + esc(g.note) + '</div>' : '') + '</td>' +
				'<td class="small">' + (g.expiresAt ? fmt.unix(g.expiresAt) : 'Never') + '</td>' +
				'<td class="small">' + when + '</td><td>' + status(g) + '</td>' +
				'<td class="text-end">' + (g.canRemove ? '<button type="button" class="btn btn-sm btn-outline-danger" data-remove="' + g.id + '">Remove</button>' : '') + '</td></tr>';
		}).join('');
	}

	function table(id, heading) {
		return (heading ? '<h6 class="mt-3">' + heading + '</h6>' : '') +
			'<div class="table-responsive"><table class="table table-sm align-middle mb-0"><thead><tr><th></th><th>What</th><th>Expires</th><th>Given</th><th>Status</th><th></th></tr></thead>' +
			'<tbody data-rows="' + id + '"></tbody></table></div>';
	}

	function form(chooseTarget) {
		var target = chooseTarget ? '<div class="col-12 col-md-3"><label class="form-label small mb-1">For</label>' +
			'<select class="form-select form-select-sm mb-1" data-target-type aria-label="Account or character"><option value="account">Account</option><option value="character">Character</option></select>' +
			'<input type="search" class="form-control form-control-sm" data-target placeholder="Search by name" aria-label="Who"></div>' : '';
		return '<form class="row g-2 align-items-end border rounded p-2 mb-3" data-grant-form>' + target +
			'<div class="col-12 col-md-3"><label class="form-label small mb-1">Kind</label><select class="form-select form-select-sm" data-kind>' +
			KINDS.map(function (k) { return '<option value="' + k[0] + '">' + esc(k[1]) + '</option>'; }).join('') + '</select></div>' +
			'<div class="col-12 col-md-' + (chooseTarget ? '3' : '4') + '"><label class="form-label small mb-1">What</label><input type="search" class="form-control form-control-sm" data-name placeholder="Search" aria-label="What to grant"></div>' +
			'<div class="col-6 col-md-2"><label class="form-label small mb-1">Effect</label><select class="form-select form-select-sm" data-deny><option value="">Grant</option><option value="1">Deny</option></select></div>' +
			'<div class="col-6 col-md-' + (chooseTarget ? '3' : '3') + '"><label class="form-label small mb-1">Expires (empty: never)</label><input type="datetime-local" class="form-control form-control-sm" data-expires></div>' +
			'<div class="col-12 col-md-' + (chooseTarget ? '6' : '9') + '"><input type="text" class="form-control form-control-sm" data-note maxlength="255" placeholder="Note (why)" aria-label="Note"></div>' +
			'<div class="col-12 col-md-3"><button class="btn btn-sm btn-primary w-100" type="submit">Add</button></div>' +
			'<div class="col-12 small text-body-secondary">Only what you have yourself can be picked. A deny takes it away even when the GM level allows it (never from GM 9).</div></form>';
	}

	function searchTargets(type, text) {
		var url = type === 'account' ? '/api/tables/accounts' : '/api/tables/characters';
		return api.post(url, { search: text, start: 0, length: 10 }).then(function (d) {
			return (d.data || []).map(function (r) {
				return { value: String(r.id), label: r.name, detail: type === 'account' ? 'Account ' + r.id + ' · GM ' + (r.gm_level || 0) : 'Character ' + r.id + (r.account_name ? ' · account ' + r.account_name : '') };
			});
		});
	}

	window.Grants = {
		mount: function (el, target) {
			var chooseTarget = !target;
			el.innerHTML = '<div data-form></div>' + table('active', chooseTarget ? 'In force' : '') +
				'<details class="mt-3"' + (chooseTarget ? ' open' : '') + '><summary class="small">History (removed and expired ones too)</summary>' + table('history', '') + '</details>';
			var formHost = el.querySelector('[data-form]');
			var nameSelect = null, targetSelect = null;

			function load() {
				var url = chooseTarget ? '/api/grants' : '/api/grants?' + target.type + '=' + encodeURIComponent(target.id);
				return api.get(url).then(function (d) {
					if (!d.success) {
						el.querySelector('[data-rows="active"]').innerHTML = '<tr><td colspan="6" class="text-danger">' + esc(d.error || 'Could not load the grants') + '</td></tr>';
						return;
					}
					var active = chooseTarget ? d.active : d.grants.filter(function (g) { return g.status === 'active'; });
					var history = chooseTarget ? d.history : d.grants;
					el.querySelector('[data-rows="active"]').innerHTML = rows(active, { showTarget: chooseTarget, empty: 'Nothing granted or denied.' });
					el.querySelector('[data-rows="history"]').innerHTML = rows(history, { showTarget: chooseTarget, empty: 'No grants yet.' });
					if ((chooseTarget || d.canManage) && !formHost.firstChild) setUpForm();
				});
			}

			function setUpForm() {
				formHost.innerHTML = form(chooseTarget);
				var f = formHost.querySelector('form');
				var kind = f.querySelector('[data-kind]');
				nameSelect = SearchSelect(f.querySelector('[data-name]'), {
					search: function (text) {
						return loadCatalog().then(function (d) { return choices(d, kind.value, text); }).catch(function (e) { toast(e.message, 'danger'); return []; });
					}
				});
				kind.addEventListener('change', function () { nameSelect.set('', ''); });
				if (chooseTarget) {
					var type = f.querySelector('[data-target-type]');
					targetSelect = SearchSelect(f.querySelector('[data-target]'), { search: function (text) { return searchTargets(type.value, text); } });
					type.addEventListener('change', function () { targetSelect.set('', ''); });
				}
				f.addEventListener('submit', function (e) {
					e.preventDefault();
					var who = chooseTarget ? { type: f.querySelector('[data-target-type]').value, id: targetSelect.input.dataset.value } : target;
					var name = nameSelect.input.dataset.value;
					if (!who.id) return toast('Pick an account or character', 'warning');
					if (!name) return toast('Pick what to grant', 'warning');
					var expires = f.querySelector('[data-expires]').value;
					var body = { targetType: who.type, target: who.id, kind: kind.value, name: name, deny: f.querySelector('[data-deny]').value === '1',
						expiresAt: expires ? Math.floor(new Date(expires).getTime() / 1000) : 0, note: f.querySelector('[data-note]').value };
					api.action('/api/grants', body).then(function (r) {
						toast(r.message, 'success');
						nameSelect.set('', '');
						f.querySelector('[data-note]').value = '';
						load();
					}).catch(function () {});
				});
			}

			el.addEventListener('click', function (e) {
				var button = e.target.closest('[data-remove]');
				if (!button || !confirm('Remove this grant? It stops applying at once.')) return;
				api.action('/api/grants/' + button.dataset.remove + '/remove', {}).then(function (r) { toast(r.message, 'success'); load(); }).catch(function () {});
			});
			if (window.Live) Live.on('grants', Live.throttle(load, 500));
			load();
		}
	};
})();
