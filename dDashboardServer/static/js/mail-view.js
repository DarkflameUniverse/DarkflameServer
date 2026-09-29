/**
 * How one in-game mail row is shown on the Mail page and in a character's mailbox: who sent it and to whom (linked to
 * their characters and accounts), its subject and body as the client shows them (locale keys translated), the
 * attachment with its icon, and its state (unread, read, attachment waiting or claimed, deleted by the player).
 * Rows come from /api/tables/mail and /api/characters/:id/mail.
 */
(function () {
	'use strict';

	function account(id) {
		return id ? ' <a class="small text-body-secondary" href="/accounts/' + esc(id) + '" title="Their account">(account)</a>' : '';
	}

	var MailView = {
		// Sender: a player's character, the game, or a staff member ("[GM] name")
		sender: function (m, withAccount) {
			if (m.sender_id && m.sender_id !== '0') return fmt.character(m.sender_id, m.sender_name) + (withAccount ? account(m.sender_account_id) : '');
			return '<span title="' + esc(m.sender_kind === 'staff' ? 'Sent from the dashboard' : 'Sent by the game') + '">' + esc(m.sender_text) + '</span> ' +
				fmt.badge(m.sender_kind === 'staff' ? 'Staff' : 'Game', m.sender_kind === 'staff' ? 'warning' : 'secondary');
		},

		receiver: function (m, withAccount) {
			return fmt.character(m.receiver_id, m.receiver_name) + (withAccount ? account(m.receiver_account_id) : '');
		},

		attachment: function (m, large) {
			var a = m.attachment;
			if (!a) return '<span class="text-body-secondary">-</span>';
			var size = large ? 40 : 28;
			var icon = a.lot ? '<img src="/api/icon/' + esc(a.lot) + '" width="' + size + '" height="' + size + '" class="me-1 align-middle" alt="" loading="lazy">' : '';
			var name = a.lot ? esc(a.count) + '× ' + esc(a.name || ('LOT ' + a.lot)) + ' <span class="small text-body-secondary">(' + esc(a.lot) + ')</span>'
				: esc(a.count) + ' item(s)';
			var state = a.claimed ? ' ' + fmt.badge('Claimed', 'secondary') : ' ' + fmt.badge('Waiting', 'info');
			return '<span class="text-nowrap">' + icon + name + '</span>' + state;
		},

		state: function (m) {
			if (m.deleted_at) return '<span title="Deleted by the player ' + esc(fmt.unix(m.deleted_at)) + '">' + fmt.badge('Deleted', 'danger') + '</span>';
			return m.read ? fmt.badge('Read', 'secondary') : fmt.badge('Unread', 'primary');
		},

		// The subject as shown, with the stored text on hover when it had locale keys
		subject: function (m) {
			var title = m.subject_text !== m.subject ? ' title="Stored as ' + esc(m.subject) + '"' : '';
			return '<span' + title + '>' + esc(m.subject_text || '(no subject)') + '</span>';
		},

		// The whole mail, for a dialog or a mailbox list
		card: function (m) {
			var stored = m.subject_text !== m.subject || m.body_text !== m.body || m.sender_text !== m.sender_name;
			var a = m.attachment;
			var rows = [
				['From', MailView.sender(m, true)],
				['To', MailView.receiver(m, true)],
				['Sent', esc(fmt.unix(m.time_sent))],
				['State', m.deleted_at ? fmt.badge('Deleted by the player', 'danger') + ' ' + esc(fmt.unix(m.deleted_at)) + ' <span class="small text-body-secondary">(' + (m.read ? 'read' : 'never read') + ' before; the player no longer sees it)</span>'
					: (m.read ? fmt.badge('Read', 'secondary') : fmt.badge('Unread', 'primary'))]
			];
			if (a) {
				rows.push(['Attachment', MailView.attachment(m, true)]);
				if (a.id && a.id !== '0') rows.push(['Item ID', '<code>' + esc(a.id) + '</code>' + (a.subkey && a.subkey !== '0' ? ' <span class="small text-body-secondary">subkey <code>' + esc(a.subkey) + '</code></span>' : '')]);
				if (a.config) rows.push(['Item data', '<pre class="small mb-0">' + esc(a.config) + '</pre>']);
			}
			return '<div class="border rounded p-2 mb-2' + (m.deleted_at ? ' border-danger-subtle' : '') + '">' +
				'<div class="d-flex justify-content-between gap-2"><strong>' + MailView.subject(m) + '</strong><span class="small text-body-secondary text-nowrap">#' + esc(m.id) + '</span></div>' +
				'<dl class="row small mb-2 mt-1">' + rows.map(function (r) { return '<dt class="col-sm-3">' + r[0] + '</dt><dd class="col-sm-9 mb-1">' + r[1] + '</dd>'; }).join('') + '</dl>' +
				'<div style="white-space: pre-wrap;">' + esc(m.body_text) + '</div>' +
				(stored ? '<details class="small mt-2"><summary class="text-body-secondary">Stored text (locale keys)</summary><div class="font-monospace" style="white-space: pre-wrap;">' +
					esc('From: ' + m.sender_name + '\nSubject: ' + m.subject + '\n\n' + m.body) + '</div></details>' : '') +
				'</div>';
		}
	};

	window.MailView = MailView;
})();
