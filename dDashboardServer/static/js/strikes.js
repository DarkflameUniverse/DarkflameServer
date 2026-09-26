/**
 * The Strikes card on an account's page: every strike with what it was for, who gave it and whether it still counts;
 * giving one by hand and revoking one (staff with the permissions). Players see their own, read-only.
 */
(function () {
	'use strict';

	var card = document.getElementById('strikesCard');
	if (!card) return;
	// A player's own card is read-only and doesn't name staff; staff see their own like anyone else's.
	// Revoking needs the server's say that this account may be managed (on your own account: self_moderation).
	var accountId = card.dataset.account, isSelf = !!card.dataset.self && !DASH.can('accounts_notes'), canManage = !!card.dataset.canManage;
	var url = isSelf ? '/api/account/strikes' : '/api/accounts/' + accountId + '/strikes';

	function render(d) {
		document.getElementById('strikeCount').textContent = d.active;
		document.getElementById('strikeCount').className = 'badge rounded-pill align-middle text-bg-' + (d.active ? 'danger' : 'secondary');
		document.getElementById('strikeHelp').textContent = (isSelf ? 'Given by staff when something you made was rejected or a score removed and they decided it deserved one. ' : '') +
			(d.expiryDays ? 'Strikes count for ' + d.expiryDays + ' days; older ones stay listed.' : 'Strikes count until revoked.');
		document.getElementById('strikeList').innerHTML = d.strikes.map(function (s) {
			var where = s.character_id !== '0' ? ' · ' + fmt.character(s.character_id, s.character_name) : '';
			var state = s.revoked_at ? fmt.badge('Revoked', 'secondary') : s.counts ? fmt.badge('Counts', 'danger') : fmt.badge('Expired', 'secondary');
			var revoke = d.canRevoke && canManage && !s.revoked_at ? ' <button type="button" class="btn btn-sm btn-link p-0 ms-2" data-revoke="' + esc(s.id) + '">Revoke</button>' : '';
			return '<li class="border-bottom py-2' + (s.counts ? '' : ' text-body-secondary') + '">' +
				'<div class="d-flex flex-wrap justify-content-between gap-2"><div>' + state + ' <strong>' + esc(s.source_name) + '</strong>' +
				(s.subject ? ': ' + esc(s.subject) : '') + where + '</div>' +
				'<div class="small text-body-secondary">' + esc(fmt.unix(s.created_at)) + (isSelf ? '' : ' by ' + esc(s.given_by)) + revoke + '</div></div>' +
				(s.reason ? '<div class="small">' + esc(s.reason) + '</div>' : '') +
				(s.revoked_at ? '<div class="small text-body-secondary">Revoked ' + esc(fmt.unix(s.revoked_at)) + (isSelf ? '' : ' by ' + esc(s.revoked_by)) +
					(s.revoke_reason ? ': ' + esc(s.revoke_reason) : '') + '</div>' : '') +
				'</li>';
		}).join('') || '<li class="text-body-secondary">No strikes.</li>';
	}

	function load() {
		return api.get(url).then(function (d) { if (d.success) render(d); });
	}

	card.addEventListener('click', function (e) {
		var revoke = e.target.closest('[data-revoke]');
		if (revoke) {
			Decide({ title: 'Revoke this strike', text: 'It stays on the record but stops counting.', reasonLabel: 'Why (for the record)', action: 'Revoke' }).then(function (choice) {
				if (!choice) return;
				api.action('/api/strikes/' + revoke.dataset.revoke + '/revoke', { reason: choice.reason }, 'Strike revoked').then(load).catch(function () {});
			});
		}
		if (e.target.closest('#giveStrike')) {
			Decide({ title: 'Give a strike', text: 'For something not covered by rejecting a name, pet name or property or removing a score.',
				reasonLabel: 'Why', reasonRequired: true, action: 'Give strike' }).then(function (choice) {
				if (!choice) return;
				api.action('/api/accounts/' + accountId + '/strikes', { reason: choice.reason }).then(function (d) { toast(d.message, 'success'); load(); }).catch(function () {});
			});
		}
	});

	if (window.Live) Live.on('strikes', function (e) { if (!e.id || String(e.id) === String(accountId)) load(); });
	load();
})();
