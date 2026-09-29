/**
 * The chat histories (chat_history.jinja2): one guild's chat (mode guild), team chat by team (mode teams) and one
 * character's whispers by conversation (mode whispers). Conversations load newest page first; Older loads the page
 * before. #team=<id> and #with=<character id> open that team or conversation (links from the Chat Log).
 */
(function () {
	'use strict';

	var page = document.getElementById('chatHistoryPage');
	var mode = page.dataset.mode;
	var id = page.dataset.id;
	var PAGE = 100, LIST_PAGE = 50;
	var nf = new Intl.NumberFormat();

	var rows = document.getElementById('threadRows');
	var flagging = ChatFlagging.create(document.getElementById('threadTable'), document.getElementById('flagBar'), function () { reloadThread(); });
	var thread = null;   // {url, oldest, messages}
	var list = { offset: 0, items: [], selected: null };

	function hashValue(name) { var m = location.hash.match(new RegExp('[#&]' + name + '=([^&]*)')); return m ? decodeURIComponent(m[1]) : ''; }

	// ---- the conversation ----

	function line(m) {
		var where = m.zone_id ? '<div class="small text-body-secondary">' + esc(m.zone_name || ('Zone ' + m.zone_id)) + '</div>' : '';
		return '<tr class="chat-line"><td style="width: 1.5rem">' + ChatFlagging.checkbox(m) + '</td>' +
			'<td class="when">' + esc(fmt.unix(m.time)) + '</td>' +
			'<td class="who">' + (m.sender_id !== '0' ? fmt.character(m.sender_id, m.sender_name) : esc(m.sender_name)) + where + '</td>' +
			'<td>' + ChatFlagging.text(m) + ChatFlagging.flagLink(m) + '</td>' +
			'<td class="text-end">' + (m.account_id ? '<a class="btn btn-sm btn-outline-secondary" href="/accounts/' + esc(m.account_id) + '">Account</a>' : '') + '</td></tr>';
	}

	function render() {
		if (!thread) return;
		rows.innerHTML = thread.messages.length ? thread.messages.map(line).join('')
			: '<tr><td class="text-body-secondary">No messages (chat is kept for log_chat_days).</td></tr>';
		document.getElementById('olderBox').classList.toggle('d-none', !thread.more);
		flagging.sync();
	}

	function load(url, older) {
		var query = '?limit=' + PAGE + (older && thread && thread.messages.length ? '&before=' + thread.messages[0].id : '');
		return api.get(url + query).then(function (d) {
			if (!d.success) { rows.innerHTML = '<tr><td class="text-danger">' + esc(d.error || 'Could not load the chat') + '</td></tr>'; return d; }
			flagging.remember(d.messages);
			if (older && thread && thread.url === url) thread.messages = d.messages.concat(thread.messages);
			else thread = { url: url, messages: d.messages };
			thread.more = d.has_more;
			render();
			if (!older) window.scrollTo({ top: document.getElementById('threadTable').getBoundingClientRect().bottom + window.scrollY - window.innerHeight + 80 });
			return d;
		});
	}

	function reloadThread() { if (thread) load(thread.url, false); }

	document.getElementById('older').addEventListener('click', function () { if (thread) load(thread.url, true); });

	function setLinks(html) { document.getElementById('threadLinks').innerHTML = html; }

	// ---- guild ----

	if (mode === 'guild') {
		load('/api/chat/guild/' + encodeURIComponent(id), false).then(function (d) {
			if (!d || !d.success) return;
			document.getElementById('threadTitle').textContent = d.guild.exists ? d.guild.name : 'Guild ' + id + ' (disbanded)';
			setLinks(d.guild.exists && DASH.can('guilds_manage') ? '<a class="btn btn-sm btn-outline-secondary" href="/guilds#guild=' + esc(id) + '">Members and history</a>' : '');
		}).catch(function () {});
		return;
	}

	// ---- teams and whisper conversations: the list on the left ----

	var listEl = document.getElementById('threadList');

	function itemHtml(item) {
		var active = list.selected === item.key;
		return '<a class="list-group-item list-group-item-action' + (active ? ' active' : '') + '" data-key="' + esc(item.key) + '">' +
			'<div class="d-flex justify-content-between align-items-center gap-2"><span class="fw-semibold text-truncate">' + esc(item.title) + '</span>' +
			'<span class="badge rounded-pill text-bg-secondary" title="Messages">' + nf.format(item.messages) + '</span></div>' +
			'<div class="meta">' + esc(item.meta) + '</div></a>';
	}

	function renderList(total) {
		listEl.innerHTML = list.items.length ? list.items.map(itemHtml).join('') : '<div class="list-group-item small text-body-secondary">None found.</div>';
		document.getElementById('listCount').textContent = nf.format(total);
		document.getElementById('listMoreBox').classList.toggle('d-none', list.items.length >= total);
	}

	function span(first, last) { return fmt.unix(first) + (last !== first ? ' to ' + fmt.unix(last) : ''); }

	function loadList(more) {
		if (!more) { list.offset = 0; list.items = []; }
		var url = mode === 'teams'
			? '/api/chat/teams?limit=' + LIST_PAGE + '&offset=' + list.offset + '&character=' + encodeURIComponent(document.getElementById('teamCharacter').value.trim())
			: '/api/characters/' + encodeURIComponent(id) + '/whispers?limit=' + LIST_PAGE + '&offset=' + list.offset;
		return api.get(url).then(function (d) {
			if (!d.success) { listEl.innerHTML = '<div class="list-group-item small text-danger">' + esc(d.error || 'Could not load') + '</div>'; return; }
			var items = mode === 'teams'
				? d.teams.map(function (t) { return { key: t.team_id, title: t.senders.join(', ') || 'Team', messages: t.messages, meta: span(t.first_time, t.last_time), team: t }; })
				: d.partners.map(function (p) { return { key: p.character_id, title: p.name || p.character_id, messages: p.messages, meta: span(p.first_time, p.last_time), partner: p }; });
			list.items = list.items.concat(items);
			list.offset += items.length;
			renderList(d.total);
		});
	}

	function open(key) {
		list.selected = key;
		listEl.querySelectorAll('[data-key]').forEach(function (el) { el.classList.toggle('active', el.getAttribute('data-key') === key); });
		var item = list.items.filter(function (i) { return i.key === key; })[0];
		flagging.clear();
		try { history.replaceState(null, '', '#' + (mode === 'teams' ? 'team' : 'with') + '=' + key); } catch (e) {}
		if (mode === 'teams') {
			document.getElementById('threadTitle').textContent = item ? 'Team: ' + item.title : 'Team ' + key;
			setLinks('');
			return load('/api/chat/team/' + encodeURIComponent(key), false).catch(function () {});
		}
		var name = item ? item.title : key;
		document.getElementById('threadTitle').textContent = 'With ' + name;
		setLinks('<a class="btn btn-sm btn-outline-secondary" href="/characters/' + esc(key) + '">' + esc(name) + '</a>' +
			'<a class="btn btn-sm btn-outline-secondary" href="/characters/' + esc(key) + '/whispers">Their whispers</a>');
		return load('/api/characters/' + encodeURIComponent(id) + '/whispers/' + encodeURIComponent(key), false).catch(function () {});
	}

	listEl.addEventListener('click', function (e) {
		var item = e.target.closest('[data-key]');
		if (item) open(item.getAttribute('data-key'));
	});
	document.getElementById('listMore').addEventListener('click', function () { loadList(true).catch(function () {}); });
	var teamCharacter = document.getElementById('teamCharacter');
	if (teamCharacter && hashValue('character')) teamCharacter.value = hashValue('character');
	if (teamCharacter) teamCharacter.addEventListener('change', function () { loadList(false).catch(function () {}); });

	var start = hashValue(mode === 'teams' ? 'team' : 'with');
	loadList(false).then(function () {
		if (start) open(start);
		// Whispers are only opened on purpose: each one opened is audited
		else if (list.items.length && mode === 'teams') open(list.items[0].key);
	}).catch(function () {});
})();
