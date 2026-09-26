/**
 * The Linked accounts card on an account's page: other accounts that share a play key, email address or login address
 * with this one (from what the servers store; addresses themselves are never shown), and how many player reports are
 * about this account.
 */
(function () {
	'use strict';

	var card = document.getElementById('linksCard');
	if (!card) return;
	var accountId = card.dataset.account;

	function detail(l) {
		if (l.link === 'PLAY_KEY') return l.name + (l.shared > 2 ? ' (' + l.shared + ' accounts use it)' : '');
		if (l.link === 'LOGIN_ADDRESS') return l.name + (l.shared > 1 ? ' (' + l.shared + ')' : '') + (l.last_seen ? ', last ' + fmt.unix(l.last_seen) : '');
		return l.name;
	}

	function load() {
		api.get('/api/accounts/' + accountId + '/links').then(function (d) {
			if (!d.success) return;
			var html = d.accounts.map(function (a) {
				return '<li class="list-group-item d-flex flex-wrap justify-content-between align-items-center gap-2">' +
					'<div><a href="/accounts/' + esc(a.account_id) + '">' + esc(a.name) + '</a>' +
					(a.gm_level ? ' ' + fmt.badge(Labels.name('gmLevels', a.gm_level) || ('GM ' + a.gm_level), 'info') : '') +
					(a.banned ? ' ' + fmt.badge('Banned', 'danger') : '') + '</div>' +
					'<div class="d-flex flex-wrap gap-1">' + a.links.map(function (l) { return fmt.badge(detail(l), 'secondary'); }).join('') + '</div></li>';
			}).join('') || '<li class="list-group-item text-body-secondary">No other account shares a play key, email address or login address with this one.</li>';
			document.getElementById('linkList').innerHTML = html;
			document.getElementById('linkCount').textContent = d.accounts.length;
			var note = d.loginAddresses + ' login address' + (d.loginAddresses === 1 ? '' : 'es') + ' on record.';
			if (d.canSeeReports) {
				note += ' <a href="/player_reports?status=&account=' + esc(accountId) + '">' + d.reports + ' player report' + (d.reports === 1 ? '' : 's') + ' about this account</a>' +
					(d.openReports ? ' (' + d.openReports + ' open)' : '') + '.';
			}
			document.getElementById('linkNote').innerHTML = note;
		}).catch(function () {});
	}

	if (window.Live) Live.on('accounts', load);
	if (window.Live) Live.on('player_reports', load);
	load();
})();
