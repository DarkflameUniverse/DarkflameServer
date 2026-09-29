/**
 * The Chat Flags page: the queue of flagged chat (open first by default), and one flag in detail: its copy of the chat,
 * links to the character, account (mute, warn, ban), their player reports and the conversation, its history, and
 * (with chat_flag_review) the review form. #flag=<id> opens a flag, #character= and #account= filter the queue.
 */
(function () {
	'use strict';

	var CHANNELS = { zone: ['Zone', 'secondary'], whisper: ['Whisper', 'info'], team: ['Team', 'primary'], guild: ['Guild', 'success'], web: ['Web', 'warning'] };
	var STATUS = { open: ['Open', 'warning'], actioned: ['Actioned', 'success'], dismissed: ['Dismissed', 'secondary'] };
	var ACTIONS = { created: 'Flagged', actioned: 'Marked actioned', dismissed: 'Dismissed', reopened: 'Reopened', note: 'Changed the note', report: 'Linked a report', comment: 'Commented' };
	var NEEDS = { whisper: 'chat_dms', team: 'chat_private', guild: 'chat_private' };
	var state = { status: 'open', character: '', account: '' };
	var shown = null;

	function hashValue(name) { var m = location.hash.match(new RegExp('[#&]' + name + '=([^&]*)')); return m ? decodeURIComponent(m[1]) : ''; }
	if (hashValue('character')) { state.character = hashValue('character'); document.getElementById('flagCharacter').value = state.character; }
	if (hashValue('account')) { state.account = hashValue('account'); document.getElementById('flagAccount').value = state.account; }
	if (state.character || state.account || hashValue('flag')) {
		state.status = '';
		document.getElementById('flagStatusAll').checked = true;
	}

	function channelBadge(c) { var b = CHANNELS[c] || [c, 'secondary']; return fmt.badge(b[0], b[1]); }
	function statusBadge(s) { var b = STATUS[s] || [s, 'secondary']; return fmt.badge(b[0], b[1]); }
	function about(f) {
		return (f.character_id !== '0' ? fmt.character(f.character_id, f.character_name) : '<span class="text-body-secondary">-</span>') +
			(f.account_id ? '<div class="small"><a href="/accounts/' + esc(f.account_id) + '">Account ' + esc(f.account_id) + '</a></div>' : '');
	}
	function excerpt(f) {
		return f.redacted ? '<span class="text-body-secondary fst-italic">Hidden: needs ' + esc(NEEDS[f.channel] || 'another permission') + '</span>' : esc(f.excerpt);
	}

	var table = serverTable('#flagsTable', '/api/tables/chat_flags', [
		{ data: 'id', orderable: false },
		{ data: 'created_at', orderable: false, render: function (d, t, row) { return esc(fmt.unix(d)) + '<div class="small text-body-secondary">by ' + esc(row.created_by) + '</div>'; } },
		{ data: 'channel', orderable: false, render: channelBadge },
		{ data: 'character_id', orderable: false, render: function (d, t, row) { return about(row); } },
		{ data: 'excerpt', orderable: false, render: function (d, t, row) { return excerpt(row); } },
		{ data: 'note', orderable: false, render: function (d) { return esc(d); } },
		{ data: 'status', orderable: false, render: function (d, t, row) {
			return statusBadge(d) + (row.updated_by ? '<div class="small text-body-secondary">' + esc(row.updated_by) + ', ' + esc(fmt.unix(row.updated_at)) + '</div>' : '');
		} },
		{ data: null, orderable: false, render: function (d) { return '<button type="button" class="btn btn-sm btn-outline-primary" data-flag="' + esc(d.id) + '">Review</button>'; } }
	], {
		liveTable: 'chat_flags',
		dataTable: { order: [], searching: false },
		extra: function () { return { status: state.status, character: state.character, account: state.account }; }
	});

	function conversationLink(f, messages) {
		var first = messages.filter(function (m) { return m.flagged; })[0];
		if (!first || f.redacted) return '';
		if (f.channel === 'whisper' && first.sender_id !== '0' && first.recipient_id !== '0') return '/characters/' + first.sender_id + '/whispers#with=' + first.recipient_id;
		if (f.channel === 'team' && first.team_id !== '0') return '/chat_log/teams#team=' + first.team_id;
		if (f.channel === 'guild' && first.guild_id !== '0') return '/chat_log/guild/' + first.guild_id;
		if (f.channel === 'zone' && first.zone_id) return '/chat_log#world=' + first.zone_id + ':' + first.instance_id;
		return '';
	}

	function show(id) {
		return api.get('/api/chat/flags/' + encodeURIComponent(id)).then(function (d) {
			if (!d.success) { toast(d.error || 'Could not load the flag', 'danger'); return; }
			var f = d.flag;
			shown = f;
			try { history.replaceState(null, '', '#flag=' + f.id); } catch (e) {}
			document.getElementById('flagDetail').classList.remove('d-none');
			document.getElementById('flagTitle').innerHTML = 'Chat flag ' + esc(f.id) + ' ' + statusBadge(f.status) + ' ' + channelBadge(f.channel);
			document.getElementById('flagSubtitle').innerHTML = 'Flagged by ' + esc(f.created_by) + ', ' + esc(fmt.unix(f.created_at)) +
				'. Messages from ' + esc(fmt.unix(f.first_time)) + (f.last_time !== f.first_time ? ' to ' + esc(fmt.unix(f.last_time)) : '') + '.';
			var thread = conversationLink(f, d.messages);
			var links = '';
			if (f.character_id !== '0') links += '<a class="btn btn-sm btn-outline-secondary" href="/characters/' + esc(f.character_id) + '">' + esc(f.character_name || 'Character') + '</a>';
			if (f.account_id) {
				links += '<a class="btn btn-sm btn-outline-danger" href="/accounts/' + esc(f.account_id) + '" title="Mute, warn, strike or ban from the account page">Account: mute, warn, ban</a>';
				if (DASH.can('player_reports_view')) links += '<a class="btn btn-sm btn-outline-secondary" href="/player_reports?status=&account=' + esc(f.account_id) + '">Reports about them</a>';
				links += '<a class="btn btn-sm btn-outline-secondary" href="/chat_flags#account=' + esc(f.account_id) + '">Their other flags</a>';
			}
			if (f.player_report_id && DASH.can('player_reports_view')) links += '<a class="btn btn-sm btn-outline-secondary" href="/player_reports?status=">Report ' + esc(f.player_report_id) + '</a>';
			if (thread) links += '<a class="btn btn-sm btn-outline-secondary" href="' + esc(thread) + '">Conversation</a>';
			document.getElementById('flagLinks').innerHTML = links;

			document.getElementById('flagMessages').innerHTML = d.messages.length ? d.messages.map(function (m) {
				return '<tr class="' + (m.flagged ? 'table-warning' : '') + '"><td class="text-nowrap small text-body-secondary">' + esc(fmt.unix(m.time)) + '</td>' +
					'<td class="text-nowrap">' + (m.sender_id !== '0' ? fmt.character(m.sender_id, m.sender_name) : esc(m.sender_name)) +
					(m.recipient_id !== '0' ? ' &rarr; ' + fmt.character(m.recipient_id, m.recipient_name) : '') + '</td>' +
					'<td>' + ChatText(m) + '</td></tr>';
			}).join('') : '<tr><td class="text-body-secondary">No copy of the chat.</td></tr>';

			document.getElementById('flagEvents').innerHTML = d.events.map(function (e) {
				return '<li class="mb-2"><span class="text-body-secondary">' + esc(fmt.unix(e.time)) + '</span> <strong>' + esc(e.actor) + '</strong> ' +
					esc((ACTIONS[e.action] || e.action).toLowerCase()) + (e.detail ? '<div>' + esc(e.detail) + '</div>' : '') + '</li>';
			}).join('') || '<li class="text-body-secondary">Nothing yet.</li>';

			var form = document.getElementById('flagReview');
			form.classList.toggle('d-none', !d.canReview || f.redacted);
			var radio = document.querySelector('input[name="reviewStatus"][value="' + f.status + '"]');
			if (radio) radio.checked = true;
			document.getElementById('reviewNote').value = f.note;
			document.getElementById('reviewReport').value = f.player_report_id || '';
			document.getElementById('reviewComment').value = '';
			document.getElementById('flagDetail').scrollIntoView({ behavior: 'smooth', block: 'start' });
		}).catch(function () {});
	}

	function ChatText(m) {
		if (m.redacted) return '<span class="text-body-secondary fst-italic">Hidden: reading ' + esc(m.channel) + ' chat needs ' + esc(NEEDS[m.channel] || 'another permission') + '</span>';
		return (m.blocked ? fmt.badge('Stopped by the filter', 'danger') + ' ' : m.filtered ? fmt.badge('Filter words', 'warning') + ' ' : '') + esc(m.message);
	}

	document.getElementById('flagReview').addEventListener('submit', function (e) {
		e.preventDefault();
		if (!shown) return;
		var status = document.querySelector('input[name="reviewStatus"]:checked');
		var body = {
			status: status ? status.value : shown.status,
			note: document.getElementById('reviewNote').value,
			player_report_id: document.getElementById('reviewReport').value.trim() || '0',
			comment: document.getElementById('reviewComment').value
		};
		var id = shown.id;
		api.action('/api/chat/flags/' + id, body, 'Saved').then(function () {
			table.ajax.reload(null, false);
			show(id);
		}).catch(function () {});
	});

	$('#flagsTable').on('click', '[data-flag]', function () { show(this.getAttribute('data-flag')); });
	document.getElementById('statusFilter').addEventListener('change', function (e) {
		if (e.target.name !== 'flagStatus') return;
		state.status = e.target.value;
		table.ajax.reload();
	});
	document.getElementById('flagCharacter').addEventListener('change', function () { state.character = this.value.trim(); table.ajax.reload(); });
	document.getElementById('flagAccount').addEventListener('change', function () { state.account = this.value.trim(); table.ajax.reload(); });

	if (hashValue('flag')) show(hashValue('flag'));
})();
