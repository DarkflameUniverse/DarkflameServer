/**
 * The Chat Log page: the chat table, and a list of the world servers running now. Picking one narrows the chat to that
 * server and shows who's in it, how busy its chat is, and what can be done there (say something, shut it down).
 * The selection is kept in the address (#world=1200:2) so it can be linked to, e.g. from the home page; #character=,
 * #account= and #search= fill in those filters (links from character and account pages).
 * Messages can be picked and flagged for review (chat-flagging.js); Context shows the conversation around a message.
 */
(function () {
	'use strict';

	var page = document.getElementById('chatLogPage');
	var can = { send: !!page.dataset.canSend, players: !!page.dataset.canPlayers, worlds: !!page.dataset.canWorlds };
	var CHANNELS = { zone: ['Zone', 'secondary'], whisper: ['Whisper', 'info'], team: ['Team', 'primary'], guild: ['Guild', 'success'], web: ['Web', 'warning'] };
	var nf = new Intl.NumberFormat();

	var state = {
		worlds: [],
		selected: null,   // {zone, instance} or null for every world
		filters: { channel: '', character: '', account: '', since: 0, until: 0, blocked: false }
	};
	var flagging = ChatFlagging.create('#chatTable', '#flagBar', function () { table.ajax.reload(null, false); });

	// Where the rest of a message's conversation is: the whisper thread, the team or the guild
	function conversationLink(row) {
		if (row.redacted) return '';
		if (row.channel === 'whisper' && DASH.can('chat_dms') && row.sender_id !== '0' && row.recipient_id !== '0') return '/characters/' + row.sender_id + '/whispers#with=' + row.recipient_id;
		if (row.channel === 'team' && DASH.can('chat_private') && row.team_id !== '0') return '/chat_log/teams#team=' + row.team_id;
		if (row.channel === 'guild' && DASH.can('chat_private') && row.guild_id !== '0') return '/chat_log/guild/' + row.guild_id;
		return '';
	}

	function worldKey(w) { return w.mapID + ':' + w.instanceID; }
	function selectedWorld() {
		if (!state.selected) return null;
		return state.worlds.filter(function (w) { return w.mapID === state.selected.zone && w.instanceID === state.selected.instance; })[0] || null;
	}

	// ---- the chat table ----

	var table = serverTable('#chatTable', '/api/tables/chat_log', [
		{ data: 'id', orderable: false, render: function (d, t, row) { return ChatFlagging.checkbox(row); } },
		{ data: 'time', orderable: false, render: function (d) { return esc(fmt.unix(d)); } },
		{ data: 'channel', orderable: false, render: function (d) { var c = CHANNELS[d] || [d, 'secondary']; return fmt.badge(c[0], c[1]); } },
		{ data: 'zone_id', orderable: false, render: function (d, t, row) {
			if (!d) return row.channel === 'web' ? '<span class="text-body-secondary">Every world</span>' : '';
			return fmt.zone(d, row.zone_name) + '<div class="small text-body-secondary">instance ' + esc(row.instance_id) + (row.clone_id ? ', clone ' + esc(row.clone_id) : '') + '</div>';
		} },
		{ data: 'sender_name', orderable: false, render: function (d, t, row) { return row.sender_id !== '0' ? fmt.character(row.sender_id, d) : esc(d); } },
		{ data: 'recipient_name', orderable: false, render: function (d, t, row) { return row.recipient_id !== '0' ? fmt.character(row.recipient_id, d) : ''; } },
		{ data: 'message', orderable: false, render: function (d, t, row) { return ChatFlagging.text(row) + ChatFlagging.flagLink(row); } },
		// The conversation, and the sender's account: mute, warn or ban from there
		{ data: 'account_id', orderable: false, render: function (d, t, row) {
			var suggest = row.sender_id !== '0' && row.channel !== 'web' ? AiSuggest.button('chat_message', row.id) : '';
			var thread = conversationLink(row);
			return '<div class="d-flex justify-content-end gap-1">' +
				(row.redacted ? '' : '<button type="button" class="btn btn-sm btn-outline-secondary text-nowrap" data-context="' + esc(row.id) + '">Context</button>') +
				(thread ? '<a class="btn btn-sm btn-outline-secondary text-nowrap" href="' + esc(thread) + '">Conversation</a>' : '') +
				(d ? '<a class="btn btn-sm btn-outline-secondary text-nowrap" href="/accounts/' + esc(d) + '">Account</a>' : '') + suggest + '</div>';
		} }
	], {
		liveTable: 'chat',
		dataTable: {
			language: { searchPlaceholder: 'Message or name' },
			order: [],
			drawCallback: function () { flagging.sync(); }
		},
		extra: function () {
			var body = { channel: state.filters.channel, character: state.filters.character, account: state.filters.account,
				since: state.filters.since, until: state.filters.until, blocked: state.filters.blocked };
			if (state.selected) { body.zone = state.selected.zone; body.instance = state.selected.instance; }
			return body;
		}
	});

	// ---- world servers ----

	function renderWorlds() {
		var list = document.getElementById('worldList');
		var running = state.worlds.filter(function (w) { return w.mapID !== 0; }); // character select has no chat
		document.getElementById('worldCount').textContent = running.length + ' running';
		var html = '<a class="list-group-item list-group-item-action' + (state.selected ? '' : ' active') + '" data-world="">' +
			'<div class="fw-semibold">Every world</div><div class="world-meta">All chat, including from the web</div></a>';
		html += running.map(function (w) {
			var active = state.selected && worldKey(w) === state.selected.zone + ':' + state.selected.instance;
			return '<a class="list-group-item list-group-item-action' + (active ? ' active' : '') + '" data-world="' + esc(worldKey(w)) + '">' +
				'<div class="d-flex justify-content-between align-items-center gap-2"><span class="fw-semibold">' + esc(w.propertyName || w.zoneName) + '</span>' +
				'<span class="badge rounded-pill text-bg-' + (w.players ? 'primary' : 'secondary') + '" title="Players">' + nf.format(w.players) + '</span></div>' +
				'<div class="world-meta">' + esc(w.mapID) + (w.propertyName ? ' · ' + esc(w.zoneName) : '') + ' · instance ' + esc(w.instanceID) + (w.cloneID ? ' · clone ' + esc(w.cloneID) : '') + '</div></a>';
		}).join('');
		if (!running.length) html += '<div class="list-group-item small text-body-secondary">No world servers running.</div>';
		list.innerHTML = html;
	}

	function renderPanel() {
		var panel = document.getElementById('worldPanel');
		var world = selectedWorld();
		var sendCard = document.getElementById('sendCard');
		sendCard.classList.toggle('d-none', !can.send);
		document.getElementById('sendLabel').textContent = world ? 'Say in ' + (world.propertyName || world.zoneName) + ' (instance ' + world.instanceID + ')' : 'Say in every world';
		if (!state.selected) { panel.classList.add('d-none'); return; }
		panel.classList.remove('d-none');
		if (!world) {
			document.getElementById('worldTitle').textContent = 'Zone ' + state.selected.zone + ', instance ' + state.selected.instance;
			document.getElementById('worldSubtitle').textContent = 'Not running any more: its chat is still below.';
			document.getElementById('worldActions').innerHTML = '';
			document.getElementById('worldStats').innerHTML = '';
			document.getElementById('worldPlayers').innerHTML = '';
			loadStats();
			return;
		}
		document.getElementById('worldTitle').textContent = (world.propertyName ? world.propertyName + ' · ' : '') + world.zoneName;
		document.getElementById('worldSubtitle').innerHTML = 'Zone ' + esc(world.mapID) + ' · instance ' + esc(world.instanceID) + (world.cloneID ? ' · clone ' + esc(world.cloneID) : '') +
			(world.ip ? ' · <code>' + esc(world.ip + ':' + world.port) + '</code>' : '') +
			(world.ownerId ? ' · owned by ' + fmt.character(world.ownerId, world.ownerName) : '') +
			(world.propertyId ? ' · ' + fmt.property(world.propertyId, 'property') : '');
		document.getElementById('worldActions').innerHTML = can.worlds
			? '<button type="button" class="btn btn-sm btn-outline-danger" id="worldShutdown">Shut down</button>' : '';
		loadStats();
		loadPlayers();
	}

	function statTile(label, value, tone) {
		return '<div class="col-6 col-md-3 world-stat"><div class="value' + (tone ? ' text-' + tone : '') + '">' + esc(value) + '</div><div class="label">' + esc(label) + '</div></div>';
	}

	function loadStats() {
		var query = state.selected ? '?zone=' + state.selected.zone + '&instance=' + state.selected.instance : '';
		api.get('/api/chat/stats' + query).then(function (d) {
			if (!d.success || !state.selected) return;
			var world = selectedWorld();
			document.getElementById('worldStats').innerHTML = statTile('Players now', world ? nf.format(world.players) : '-') +
				statTile('Messages, last hour', nf.format(d.lastHour)) + statTile('Messages, 24 hours', nf.format(d.today)) +
				statTile('Stopped by the filter, 24 hours', nf.format(d.blockedToday), d.blockedToday ? 'danger' : '');
		});
	}

	function loadPlayers() {
		var box = document.getElementById('worldPlayers');
		if (!can.players) { box.innerHTML = '<span class="small text-body-secondary">Seeing who is online needs the players_view permission.</span>'; return; }
		api.get('/api/players/online').then(function (d) {
			if (!d.success || !state.selected) return;
			var here = d.players.filter(function (p) { return p.zone === state.selected.zone && p.instance === state.selected.instance; });
			box.innerHTML = here.length ? here.map(function (p) { return '<span class="badge text-bg-secondary fw-normal">' + fmt.character(p.id, p.name) + '</span>'; }).join('')
				: '<span class="small text-body-secondary">Nobody is in this world right now.</span>';
		});
	}

	function loadWorlds() {
		return api.get('/api/worlds').then(function (d) {
			if (!d.success) return;
			state.worlds = d.worlds;
			renderWorlds();
			renderPanel();
		});
	}

	function select(key) {
		if (!key) state.selected = null;
		else { var parts = key.split(':'); state.selected = { zone: Number(parts[0]), instance: Number(parts[1]) }; }
		try { history.replaceState(null, '', state.selected ? '#world=' + key : location.pathname); } catch (e) {}
		renderWorlds();
		renderPanel();
		table.ajax.reload();
	}

	// ---- events ----

	document.getElementById('worldList').addEventListener('click', function (e) {
		var item = e.target.closest('[data-world]');
		if (item) select(item.dataset.world);
	});
	document.getElementById('chatChannel').addEventListener('change', function () { state.filters.channel = this.value; table.ajax.reload(); });
	document.getElementById('chatCharacter').addEventListener('change', function () { state.filters.character = this.value.trim(); table.ajax.reload(); });
	document.getElementById('chatBlocked').addEventListener('change', function () { state.filters.blocked = this.checked; table.ajax.reload(); });
	document.getElementById('chatAccount').addEventListener('change', function () { state.filters.account = this.value.trim(); table.ajax.reload(); });
	// datetime-local is the viewer's local time; the server takes unix seconds
	function unixOf(value) { var t = value ? new Date(value).getTime() : NaN; return isNaN(t) ? 0 : Math.floor(t / 1000); }
	document.getElementById('chatSince').addEventListener('change', function () { state.filters.since = unixOf(this.value); table.ajax.reload(); });
	document.getElementById('chatUntil').addEventListener('change', function () { state.filters.until = unixOf(this.value); table.ajax.reload(); });
	$('#chatTable').on('xhr.dt', function (e, settings, json) { if (json && json.data) flagging.remember(json.data); });

	// ---- the conversation around a message ----

	document.getElementById('chatTable').addEventListener('click', function (e) {
		var button = e.target.closest('[data-context]');
		if (button) showContext(button.getAttribute('data-context'));
	});

	function showContext(id) {
		api.get('/api/chat/messages/' + encodeURIComponent(id) + '/context?count=15').then(function (d) {
			if (!d.success) { toast(d.error || 'Could not load the conversation', 'danger'); return; }
			var modalEl = document.createElement('div');
			modalEl.className = 'modal fade';
			modalEl.tabIndex = -1;
			var thread = conversationLink(d.message);
			modalEl.innerHTML = '<div class="modal-dialog modal-xl modal-dialog-scrollable"><div class="modal-content">' +
				'<div class="modal-header"><h5 class="modal-title">Conversation around a ' + esc((CHANNELS[d.message.channel] || [d.message.channel])[0].toLowerCase()) + ' message</h5><button type="button" class="btn-close" data-bs-dismiss="modal" aria-label="Close"></button></div>' +
				'<div class="modal-body"><div class="d-flex flex-wrap gap-2 align-items-center mb-2" data-context-bar></div>' +
				'<table class="table table-sm align-middle mb-0" data-context-table><tbody>' + d.messages.map(function (m) {
					return '<tr class="' + (String(m.id) === String(id) ? 'table-warning' : '') + '"><td style="width: 1.5rem">' + ChatFlagging.checkbox(m) + '</td>' +
						'<td class="text-nowrap small text-body-secondary">' + esc(fmt.unix(m.time)) + '</td>' +
						'<td class="text-nowrap">' + (m.sender_id !== '0' ? fmt.character(m.sender_id, m.sender_name) : esc(m.sender_name)) + (m.recipient_id !== '0' ? ' &rarr; ' + fmt.character(m.recipient_id, m.recipient_name) : '') + '</td>' +
						'<td>' + ChatFlagging.text(m) + ChatFlagging.flagLink(m) + '</td></tr>';
				}).join('') + '</tbody></table></div>' +
				'<div class="modal-footer">' + (thread ? '<a class="btn btn-outline-secondary" href="' + esc(thread) + '">Whole conversation</a>' : '') +
				'<button type="button" class="btn btn-secondary" data-bs-dismiss="modal">Close</button></div></div></div>';
			document.body.appendChild(modalEl);
			var modal = new bootstrap.Modal(modalEl);
			var picker = ChatFlagging.create(modalEl.querySelector('[data-context-table]'), modalEl.querySelector('[data-context-bar]'), function () { modal.hide(); table.ajax.reload(null, false); });
			picker.remember(d.messages);
			modalEl.addEventListener('hidden.bs.modal', function () { modal.dispose(); modalEl.remove(); });
			modalEl.addEventListener('click', function (e) { if (e.target.closest('a[href]')) modal.hide(); });
			modal.show();
		}).catch(function () {});
	}
	document.getElementById('worldActions').addEventListener('click', function (e) {
		if (!e.target.closest('#worldShutdown') || !state.selected) return;
		var world = selectedWorld();
		if (!confirm('Shut down ' + (world ? world.zoneName : 'this world') + ' instance ' + state.selected.instance + '? Its players are disconnected.')) return;
		api.action('/api/worlds/shutdown', { zone: state.selected.zone, instance: state.selected.instance }, 'Shutting down').catch(function () {});
	});
	document.getElementById('chatSend').addEventListener('submit', function (e) {
		e.preventDefault();
		var input = document.getElementById('chatMessage');
		var body = { message: input.value };
		if (state.selected) { body.zone = state.selected.zone; body.instance = state.selected.instance; }
		api.action('/api/chat/send', body).then(function () { input.value = ''; }).catch(function () {});
	});

	// Worlds start and stop, and players move: keep the list and the panel current
	if (window.Live) {
		Live.on('chat', Live.throttle(function () { if (state.selected) loadStats(); }, 2000));
		Live.onStatus && Live.onStatus(Live.throttle(loadWorlds, 5000));
	}
	setInterval(loadWorlds, 15000);

	var hashValue = function (name) { var m = location.hash.match(new RegExp('[#&]' + name + '=([^&]*)')); return m ? decodeURIComponent(m[1]) : ''; };
	if (hashValue('character')) { state.filters.character = hashValue('character'); document.getElementById('chatCharacter').value = state.filters.character; }
	if (hashValue('account')) { state.filters.account = hashValue('account'); document.getElementById('chatAccount').value = state.filters.account; }
	if (state.filters.character || state.filters.account) table.ajax.reload();
	var fromHash = (location.hash.match(/world=(\d+:\d+)/) || [])[1];
	if (fromHash) { var p = fromHash.split(':'); state.selected = { zone: Number(p[0]), instance: Number(p[1]) }; }
	loadWorlds().then(function () { if (state.selected) table.ajax.reload(); });
})();
