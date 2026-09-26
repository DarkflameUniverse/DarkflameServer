/**
 * Your own account page: two-factor login setup and your characters' trade and mail history.
 */
(function () {
	'use strict';

	function $(id) { return document.getElementById(id); }
	function show(id, visible) { $(id).classList.toggle('d-none', !visible); }

	function showRecoveryCodes(codes) {
		$('recoveryCodeList').textContent = codes.join('\n');
		show('recoveryCodes', true);
		$('recoveryCodes').scrollIntoView({ behavior: 'smooth', block: 'center' });
	}

	function renderQr(uri) {
		var box = $('twoFactorQr');
		box.textContent = '';
		if (!window.qrcode) return;
		var qr = qrcode(0, 'M');
		qr.addData(uri);
		qr.make();
		// SVG with a quiet zone, drawn dark-on-light so phone cameras read it on the dark theme
		box.innerHTML = qr.createSvgTag({ cellSize: 4, margin: 4, scalable: true });
	}

	function load() {
		return api.get('/api/account/2fa').then(function (s) {
			show('twoFactorRequired', s.required && !s.enabled);
			show('twoFactorOff', !s.enabled);
			show('twoFactorOn', s.enabled);
			show('twoFactorSetup', false);
			$('twoFactorBadge').innerHTML = s.enabled ? fmt.badge('On', 'success') : fmt.badge('Off', s.required ? 'warning' : 'secondary');
			if (s.enabled) {
				$('twoFactorInfo').textContent = 'On since ' + fmt.unix(s.enabledAt) + '. ' + s.recoveryCodesLeft + ' recovery code' + (s.recoveryCodesLeft === 1 ? '' : 's') + ' left.';
				show('twoFactorDisableGroup', !s.required);
			}
		});
	}

	$('twoFactorStart').addEventListener('click', function () {
		api.action('/api/account/2fa/setup').then(function (r) {
			$('twoFactorSecret').textContent = r.secret.replace(/(.{4})/g, '$1 ').trim();
			renderQr(r.uri);
			show('twoFactorOff', false);
			show('twoFactorSetup', true);
			$('twoFactorEnableCode').focus();
		}).catch(function () {});
	});

	$('twoFactorEnable').addEventListener('click', function () {
		api.action('/api/account/2fa/enable', { code: $('twoFactorEnableCode').value.trim() }).then(function (r) {
			toast(r.message, 'success');
			showRecoveryCodes(r.recoveryCodes);
			load();
		}).catch(function () {});
	});

	$('twoFactorNewCodes').addEventListener('click', function () {
		api.action('/api/account/2fa/recovery_codes', { code: $('twoFactorManageCode').value.trim() }).then(function (r) {
			showRecoveryCodes(r.recoveryCodes);
			$('twoFactorManageCode').value = '';
			load();
		}).catch(function () {});
	});

	$('twoFactorDisable').addEventListener('click', function () {
		if (!confirm('Turn off two-factor login? Your password alone will be enough to sign in.')) return;
		api.action('/api/account/2fa/disable', { password: $('twoFactorDisablePwd').value, code: $('twoFactorManageCode').value.trim() }, 'Two-factor login is off')
			.then(function () {
				$('twoFactorDisablePwd').value = '';
				$('twoFactorManageCode').value = '';
				show('recoveryCodes', false);
				load();
			}).catch(function () {});
	});

	$('recoveryCopy').addEventListener('click', function () {
		navigator.clipboard.writeText($('recoveryCodeList').textContent).then(function () { toast('Copied', 'success'); });
	});

	// ---- Trade and mail history ----

	// Names come from the server's enum (Labels); only the colours are kept here
	var METHOD_COLOURS = { 1: 'info', 2: 'secondary', 3: 'success', 4: 'light' };
	function methodBadge(value) { return fmt.badge(Labels.name('transferMethods', value) || '?', METHOD_COLOURS[value] || 'secondary'); }

	function characterName(id, name) { return esc(name || id); }

	if (window.jQuery && document.getElementById('ownTransfers')) {
		serverTable('#ownTransfers', '/api/account/transfers', [
			{ data: 'time', orderable: false, render: function (d) { return esc(fmt.unix(d)); } },
			{ data: 'method', orderable: false, render: function (d) { return methodBadge(d); } },
			{ data: 'lot', orderable: false, render: function (d, t, row) {
				if (!d) return esc(Number(row.coins).toLocaleString()) + ' coins';
				var what = esc((row.count > 1 ? row.count + '× ' : '') + (row.name || 'LOT ' + d));
				// Staff who can see reports get the item's full trace
				return DASH.can('reports_view') && row.item_id && row.item_id !== '0' ? '<a href="/reports?object=' + esc(row.item_id) + '" title="Trace this item">' + what + '</a>' : what;
			} },
			{ data: 'from_character', orderable: false, render: function (d, t, row) { return characterName(d, row.from_name); } },
			{ data: 'to_character', orderable: false, render: function (d, t, row) { return characterName(d, row.to_name); } }
		], { dataTable: { searching: false, ordering: false, pageLength: 10, lengthChange: false }, liveTable: 'economy' });
	}

	if (location.hash === '#two-factor') $('two-factor').scrollIntoView();
	load();
})();
