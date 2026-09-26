/**
 * The Chat Log page: the chat table, and a list of the world servers running now. Picking one narrows the chat to that
 * server and shows who's in it, how busy its chat is, and what can be done there (say something, shut it down).
 * The selection is kept in the address (#world=1200:2) so it can be linked to, e.g. from the home page.
 */
(function () {
	'use strict';

	var page = document.getElementById('chatLogPage');
	var can = { send: !!page.dataset.canSend, players: !!page.dataset.canPlayers, worlds: !!page.dataset.canWorlds };
	var CHANNELS = { zone: ['Zone', 'secondary'], whisper: ['Whisper', 'info'], team: ['Team', 'primary'], web: ['Web', 'warning'] };
	var nf = new Intl.NumberFormat();

	var state = {
		worlds: [],
		selected: null,   // {zone, instance} or null for every world
		filters: { channel: '', character: '', blocked: false }
	};

	function worldKey(w) { return w.mapID + ':' + w.instanceID; }
	function selectedWorld() {
		if (!state.selected) return null;
		return state.worlds.filter(function (w) { return w.mapID === state.selected.zone && w.instanceID === state.selected.instance; })[0] || null;
	}

	// ---- the chat table ----

	var table = serverTable('#chatTable', '/api/tables/chat_log', [
		{ data: 'time', orderable: false, render: function (d) { return esc(fmt.unix(d)); } },
		{ data: 'channel', orderable: false, render: function (d) { var c = CHANNELS[d] || [d, 'secondary']; return fmt.badge(c[0], c[1]); } },
		{ data: 'zone_id', orderable: false, render: function (d, t, row) {
			if (!d) return row.channel === 'web' ? '<span class="text-body-secondary">Every world</span>' : '';
			return fmt.zone(d, row.zone_name) + '<div class="small text-body-secondary">instance ' + esc(row.instance_id) + (row.clone_id ? ', clone ' + esc(row.clone_id) : '') + '</div>';
		} },
		{ data: 'sender_name', orderable: false, render: function (d, t, row) { return row.sender_id !== '0' ? fmt.character(row.sender_id, d) : esc(d); } },
		{ data: 'recipient_name', orderable: false, render: function (d, t, row) { return row.recipient_id !== '0' ? fmt.character(row.recipient_id, d) : ''; } },
		{ data: 'message', orderable: false, render: function (d, t, row) {
			return (row.blocked ? fmt.badge('Stopped by the filter', 'danger') + ' ' : '') + '<span class="' + (row.blocked ? 'text-body-secondary' : '') + '">' + esc(d) + '</span>';
		} },
		// The sender's account: mute, warn or ban from there
		{ data: 'account_id', orderable: false, render: function (d, t, row) {
			var suggest = row.sender_id !== '0' && row.channel !== 'web' ? AiSuggest.button('chat_message', row.id) : '';
			return '<div class="d-flex justify-content-end gap-1">' + (d ? '<a class="btn btn-sm btn-outline-secondary text-nowrap" href="/accounts/' + esc(d) + '">Account</a>' : '') + suggest + '</div>';
		} }
	], {
		liveTable: 'chat',
		dataTable: { language: { searchPlaceholder: 'Message or name' } },
		extra: function () {
			var body = { channel: state.filters.channel, character: state.filters.character, blocked: state.filters.blocked };
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
			(world.propertyId ? ' · ' + fmt.link('/properties/' + world.propertyId, 'property') : '');
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

	var fromHash = (location.hash.match(/world=(\d+:\d+)/) || [])[1];
	if (fromHash) { var p = fromHash.split(':'); state.selected = { zone: Number(p[0]), instance: Number(p[1]) }; }
	loadWorlds().then(function () { if (state.selected) table.ajax.reload(); });
})();
