/**
 * The Mail page: every in-game mail (DataTables on /api/tables/mail), filtered by state, character or account.
 * Opens with filters from the address: /mail#character=<id>, /mail#account=<id>, /mail#state=deleted.
 */
(function () {
	'use strict';

	var filters = { state: '', character: '', account: '' };
	(location.hash.replace(/^#/, '').split('&')).forEach(function (part) {
		var kv = part.split('=');
		if (kv.length === 2 && kv[0] in filters) filters[kv[0]] = decodeURIComponent(kv[1]);
	});
	document.getElementById('mailState').value = filters.state;
	document.getElementById('mailCharacter').value = filters.character;
	document.getElementById('mailAccount').value = filters.account;

	var rows = {};
	var table = serverTable('#mailTable', '/api/tables/mail', [
		{ data: 'time_sent', orderable: false, render: function (d) { return '<span class="text-nowrap">' + esc(fmt.unix(d)) + '</span>'; } },
		{ data: 'sender_name', orderable: false, render: function (d, t, m) { return MailView.sender(m); } },
		{ data: 'receiver_name', orderable: false, render: function (d, t, m) { return MailView.receiver(m); } },
		// The subject opens the whole mail
		{ data: 'subject', orderable: false, render: function (d, t, m) {
			rows[m.id] = m;
			return '<a href="#" data-mail="' + esc(m.id) + '" title="Open the mail">' + esc(m.subject_text || '(no subject)') + '</a>';
		} },
		{ data: 'attachment', orderable: false, render: function (d, t, m) { return MailView.attachment(m); } },
		{ data: 'deleted_at', orderable: false, render: function (d, t, m) { return MailView.state(m); } }
	], {
		liveTable: 'mail',
		dataTable: { language: { searchPlaceholder: 'Subject, text or name' } },
		extra: function () { return { state: filters.state, character: filters.character, account: filters.account }; }
	});

	function setFilter(name, value) {
		filters[name] = value;
		var hash = Object.keys(filters).filter(function (k) { return filters[k]; }).map(function (k) { return k + '=' + encodeURIComponent(filters[k]); }).join('&');
		try { history.replaceState(null, '', hash ? '#' + hash : location.pathname); } catch (e) {}
		table.ajax.reload();
	}

	document.getElementById('mailState').addEventListener('change', function () { setFilter('state', this.value); });
	document.getElementById('mailCharacter').addEventListener('change', function () { setFilter('character', this.value.trim()); });
	document.getElementById('mailAccount').addEventListener('change', function () { setFilter('account', this.value.trim()); });

	document.getElementById('mailTable').addEventListener('click', function (e) {
		var open = e.target.closest('[data-mail]');
		if (!open || !rows[open.dataset.mail]) return;
		e.preventDefault();
		document.getElementById('mailModalBody').innerHTML = MailView.card(rows[open.dataset.mail]);
		bootstrap.Modal.getOrCreateInstance(document.getElementById('mailModal')).show();
	});
})();
