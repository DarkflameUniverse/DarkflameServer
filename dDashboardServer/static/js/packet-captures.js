/**
 * Packet Captures (docs/CaptureReplay.md): arm a capture for an account, a character or everything; the running and
 * saved ones; and a viewer that plays a capture back on one timeline (play, pause, seek, speed), with each packet's
 * decoded fields and bytes. While it plays, the time is sent to World 3D pages showing the same capture. The timeline
 * marks each time a captured character moved to another world server, with the zone's name.
 */
(function () {
	'use strict';
	var byId = function (id) { return document.getElementById(id); };
	var nf = new Intl.NumberFormat();
	var MAX_LOADED = 50000; // packets loaded into the viewer
	var MAX_ROWS = 400;     // table rows drawn at once (the ones up to the playhead)
	var channel = 'BroadcastChannel' in window ? new BroadcastChannel('dlu-capture-replay') : null;

	var state = {
		live: [],       // running and recently finished, from the server
		saved: [],      // saved packet captures
		capture: null,  // the one in the viewer
		records: [],    // its packets, in time order (without fields)
		duration: 0,    // ms
		playhead: 0,    // ms from its start
		playing: false,
		selected: -1,
		loading: false,
		worlds: []      // the characters' moves between world servers (the worlds route)
	};

	function bytesText(n) {
		if (n < 1024) return n + ' B';
		if (n < 1024 * 1024) return (n / 1024).toFixed(1) + ' KB';
		return (n / 1024 / 1024).toFixed(1) + ' MB';
	}
	function timeText(ms) {
		var s = Math.max(0, ms) / 1000, m = Math.floor(s / 60);
		return m + ':' + (s - m * 60).toFixed(3).padStart(6, '0');
	}
	function url(id) { return '/api/inspector/sessions/' + id; }

	// ---- target picker ----

	function target() { return document.querySelector('input[name="target"]:checked').value; }

	var picker = SearchSelect(byId('targetPicker'), {
		search: function (q) {
			return api.get('/api/inspector/targets?q=' + encodeURIComponent(q)).then(function (d) {
				var items = [];
				(d.accounts || []).forEach(function (a) {
					if (target() === 'account') {
						items.push({
							value: String(a.id), label: a.name + ' (account ' + a.id + ')' + (a.online ? ' · online' : ''),
							detail: a.characters.map(function (c) { return c.name + (c.online ? ' (online)' : ''); }).join(', ') || 'No characters'
						});
					} else {
						a.characters.forEach(function (c) {
							items.push({ value: c.id, label: c.name + ' (' + c.id + ')' + (c.online ? ' · online' : ''), detail: 'Account ' + a.name + ' (' + a.id + ')' });
						});
					}
				});
				return items;
			});
		}
	});

	function onTarget() {
		var t = target();
		byId('pickerWrap').classList.toggle('d-none', t === 'everything');
		byId('everythingHint').classList.toggle('d-none', t !== 'everything');
		byId('pickerLabel').textContent = t === 'account' ? 'Account' : 'Character';
		picker.set('', '');
	}
	document.querySelectorAll('input[name="target"]').forEach(function (r) { r.addEventListener('change', onTarget); });

	byId('armForm').addEventListener('submit', function (e) {
		e.preventDefault();
		var t = target(), body = { target: t, seconds: parseInt(byId('armSeconds').value, 10) };
		if (t !== 'everything') {
			var value = byId('targetPicker').dataset.value;
			if (!value) { toast('Pick ' + (t === 'account' ? 'an account' : 'a character') + ' from the list', 'warning'); return; }
			body[t] = value;
		}
		api.action('/api/inspector/packet-captures', body, 'Capture armed on every server').then(function (d) {
			upsert(d.capture);
			open(d.capture.id);
		}).catch(function () {});
	});

	// ---- list ----

	function upsert(capture) {
		var i = state.live.findIndex(function (c) { return c.id === capture.id; });
		if (i >= 0) state.live[i] = capture; else state.live.unshift(capture);
		if (state.capture && state.capture.id === capture.id) { state.capture = capture; renderHeader(); }
		renderList();
	}

	function renderList() {
		var seen = {}, all = [];
		state.live.concat(state.saved).forEach(function (c) { if (!seen[c.id]) { seen[c.id] = true; all.push(c); } });
		byId('captureList').innerHTML = all.length ? all.map(function (c) {
			var active = state.capture && state.capture.id === c.id;
			var badge = c.state === 'ended' ? '' : ' <span class="badge text-bg-success">capturing</span>';
			return '<div class="list-group-item list-group-item-action' + (active ? ' active' : '') + '" data-id="' + c.id + '">' +
				'<div class="d-flex justify-content-between gap-2"><span class="text-truncate">' + esc(c.describe || c.target) + '</span>' + badge + '</div>' +
				'<div class="small text-body-secondary">' + esc(fmt.unix(c.startedAt)) + ' · ' + nf.format(c.received) + ' packets · ' + esc(bytesText(c.bytes)) +
				(c.dropped ? ' · <span class="text-warning">' + nf.format(c.dropped) + ' lost</span>' : '') + '</div></div>';
		}).join('') : '<div class="list-group-item text-body-secondary small">No packet captures yet.</div>';
	}

	byId('captureList').addEventListener('click', function (e) {
		var item = e.target.closest('[data-id]');
		if (item) open(parseInt(item.dataset.id, 10));
	});

	function refreshList() {
		return Promise.all([
			api.get('/api/inspector/packet-captures').then(function (d) { state.live = d.captures || []; }),
			api.get('/api/inspector/sessions?limit=100').then(function (d) {
				state.saved = (d.sessions || []).filter(function (s) { return s.kind === 1; }).map(function (s) {
					if (!s.describe) s.describe = s.target === 'everything' ? 'everything' : s.target === 'character' ? s.characterName : s.accountName;
					return s;
				});
			})
		]).then(renderList).catch(function () {});
	}

	// ---- viewer ----

	function renderHeader() {
		var c = state.capture;
		if (!c) return;
		byId('viewTitle').textContent = 'Capture ' + c.id + ': ' + (c.describe || c.target);
		byId('viewSubtitle').textContent = 'Armed by ' + c.startedBy + ' ' + fmt.unix(c.startedAt) + ' · ' + nf.format(c.received) + ' packets, ' + bytesText(c.bytes) +
			(c.dropped ? ' · ' + nf.format(c.dropped) + ' lost on the servers' : '') + ' · ' + (c.state === 'ended' ? (c.endReason || 'ended') : 'capturing until ' + fmt.unix(c.endsAt));
		byId('stopBtn').classList.toggle('d-none', c.state === 'ended');
		byId('deleteBtn').classList.toggle('d-none', c.state !== 'ended');
		byId('bundleBtn').href = url(c.id) + '/bundle';
		byId('fixtureBtn').href = url(c.id) + '/bundle?anonymise=1';
	}

	function open(id) {
		stop();
		state.capture = state.live.concat(state.saved).filter(function (c) { return c.id === id; })[0] || { id: id, received: 0, bytes: 0 };
		state.records = [];
		state.worlds = [];
		byId('worldMarkers').innerHTML = '';
		byId('propertySummary').classList.add('d-none');
		state.selected = -1;
		history.replaceState(null, '', '#capture=' + id);
		byId('viewer').classList.remove('d-none');
		byId('emptyViewer').classList.add('d-none');
		byId('detail').classList.add('d-none');
		byId('grid').classList.add('no-detail');
		renderHeader();
		renderList();
		load(true);
	}

	// Loads the capture's packets (the new ones, when `fresh` is false and it is still running)
	function load(fresh) {
		var c = state.capture;
		if (!c || state.loading) return Promise.resolve();
		state.loading = true;
		var offset = fresh ? 0 : state.records.length;
		var step = function () {
			return api.get(url(c.id) + '/packets?fields=0&limit=2000&offset=' + offset).then(function (d) {
				if (!state.capture || state.capture.id !== c.id) return;
				if (d.success === false) { toast(d.error || 'Could not load the capture', 'danger'); return; }
				if (offset === 0) state.records = [];
				Array.prototype.push.apply(state.records, d.records);
				state.duration = d.duration || 0;
				state.capture = Object.assign(state.capture, d.capture);
				offset += d.records.length;
				if (d.records.length === 2000 && state.records.length < MAX_LOADED) return step();
			});
		};
		return step().then(function () {
			state.loading = false;
			var worlds = state.records.filter(function (r) { return r.source === 'world' && r.zone; });
			var zone = worlds.length ? worlds[0].zone : '';
			byId('world3dBtn').href = '/world3d?capture=' + c.id + (zone ? '&zone=' + zone : '');
			byId('scrub').max = String(Math.max(1, Math.ceil(state.duration)));
			if (fresh) setPlayhead(state.duration);
			else render();
			renderHeader();
			loadWorlds(c.id);
			loadProperty(c.id);
		}, function () { state.loading = false; });
	}

	// ---- the properties the capture was on ----

	function point(p) { return p && p.length === 3 ? p.map(function (v) { return v.toFixed(1); }).join(', ') : ''; }

	// What happened to the models: placed, moved (a run of moves of one model is one entry) and removed
	function modelEvents(world) {
		var out = [];
		(world.events || []).forEach(function (e) {
			if (e.kind !== 'placed' && e.kind !== 'moved' && e.kind !== 'removed') return;
			var last = out[out.length - 1];
			if (e.kind === 'moved' && last && last.kind === 'moved' && last.object === e.object) { last.moves++; last.position = e.position; return; }
			out.push(Object.assign({ moves: 1 }, e));
		});
		return out;
	}

	function eventHtml(e) {
		var what = e.object ? esc(e.name || ('LOT ' + e.lot)) + ' <span class="text-body-secondary">LOT ' + esc(e.lot) + '</span>' : 'a model';
		var text = e.kind === 'placed' ? 'Placed ' + what + (e.position ? ' at ' + esc(point(e.position)) : '') + (e.object ? '' : ' <span class="text-body-secondary">(not seen made)</span>')
			: e.kind === 'moved' ? 'Moved ' + what + (e.moves > 1 ? ' ' + e.moves + ' times' : '') + ' to ' + esc(point(e.position))
				: 'Removed ' + what + (e.reason ? ' (' + esc(e.reason) + ')' : '');
		// Jump to the packet that said it: the client's request for a removal it asked for
		var index = e.kind === 'removed' && e.asked !== undefined ? e.asked : e.i;
		return '<button type="button" class="list-group-item list-group-item-action px-2 py-1" data-packet="' + esc(index) + '" data-at="' + esc(e.t) + '">' +
			'<span class="font-monospace text-body-secondary me-2">' + timeText(e.t * 1000) + '</span>' + text + ' <span class="text-body-secondary">#' + esc(index) + '</span></button>';
	}

	function worldHtml(w) {
		var info = (w.info || [])[0], last = (w.info || [])[(w.info || []).length - 1];
		var owner = info ? info.ownerName : (w.saved ? w.saved.ownerName : '');
		var ownerId = info ? info.ownerId : (w.saved ? w.saved.ownerId : '');
		var events = modelEvents(w);
		var name = last && last.name ? last.name : (w.saved ? w.saved.name : '');
		return '<div class="card mb-2"><div class="card-body p-2">' +
			'<div><strong>' + esc(name || 'Property') + '</strong>' + (owner ? ' · owner ' + (ownerId && ownerId !== '0' ? fmt.character(ownerId, owner) : esc(owner)) : '') +
			' · ' + esc(w.zoneName || ('Zone ' + w.zone)) + ' #' + esc(w.instance) + (w.clone ? ' (clone ' + esc(w.clone) + ')' : '') +
			' · ' + nf.format(w.models.length) + ' model' + (w.models.length === 1 ? '' : 's') + ' seen' +
			(w.saved ? ' · <a href="/properties/' + esc(w.saved.id) + '">the property now</a>' : '') + '</div>' +
			(info && (w.info || []).length > 1 ? '<div class="text-body-secondary">Its property data changed ' + ((w.info || []).length - 1) + ' time(s) during the capture.</div>' : '') +
			(events.length ? '<div class="list-group list-group-flush mt-1" style="max-height: 12rem; overflow-y: auto">' + events.map(eventHtml).join('') + '</div>'
				: '<div class="text-body-secondary">No model was placed, moved or removed during the capture.</div>') +
			'</div></div>';
	}

	function loadProperty(id) {
		api.get(url(id) + '/property').then(function (d) {
			if (!state.capture || state.capture.id !== id || !d.worlds) return;
			var box = byId('propertySummary');
			box.innerHTML = d.worlds.map(worldHtml).join('');
			box.classList.toggle('d-none', !d.worlds.length);
		}).catch(function () {});
	}

	byId('propertySummary').addEventListener('click', function (e) {
		var entry = e.target.closest('[data-packet]');
		if (!entry) return;
		stop();
		setPlayhead(Number(entry.dataset.at) * 1000 + 1);
		showPacket(parseInt(entry.dataset.packet, 10));
	});

	// The timeline's world change markers (world3d-core.js draws them, as World 3D's replay does)
	function loadWorlds(id) {
		Promise.all([api.get(url(id) + '/worlds'), import('/js/world3d-core.js')]).then(function (results) {
			var d = results[0], core = results[1];
			if (!state.capture || state.capture.id !== id || !d.worlds) return;
			state.worlds = d.worlds;
			var characters = {};
			d.worlds.forEach(function (w) { characters[w.character] = true; });
			var markers = core.worldMarkers(d.worlds, state.duration / 1000);
			byId('worldMarkers').innerHTML = core.markersHtml(markers, esc, Object.keys(characters).length > 1);
		}).catch(function () {});
	}

	function filtered() {
		var name = byId('nameFilter').value.trim().toUpperCase(), source = byId('sourceFilter').value;
		if (!name && !source) return state.records;
		return state.records.filter(function (r) {
			return (!source || r.source === source) && (!name || String(r.name || '').indexOf(name) >= 0);
		});
	}

	// The rows up to the playhead (the last MAX_ROWS of them)
	function render() {
		var list = filtered(), until = 0;
		while (until < list.length && list[until].t <= state.playhead) until++;
		var from = Math.max(0, until - MAX_ROWS);
		byId('rows').innerHTML = list.slice(from, until).map(function (r) {
			var cls = (r.gap ? 'gap ' : '') + (r.toServer ? 'to-server ' : '') + (r.i === state.selected ? 'selected' : '');
			return '<tr class="' + cls + '" data-i="' + r.i + '"><td>' + r.i + '</td><td class="font-monospace">' + timeText(r.t) + '</td>' +
				'<td>' + esc(r.source + (r.source === 'world' && r.zone ? ' ' + r.zone : '')) + '</td>' +
				'<td>' + esc(r.gap ? '' : (r.from + ' → ' + r.to)) + '</td>' +
				'<td title="' + esc(r.name || '') + '">' + esc(r.name || '') + (r.cut ? ' <span class="badge text-bg-warning">cut</span>' : '') + (r.unreadable ? ' <span class="badge text-bg-danger">unreadable</span>' : '') + '</td>' +
				'<td class="text-end">' + nf.format(r.bytes) + '</td></tr>';
		}).join('');
		byId('countText').textContent = nf.format(until) + ' of ' + nf.format(list.length) + ' shown' + (list.length !== state.records.length ? ' (filtered from ' + nf.format(state.records.length) + ')' : '') +
			(state.records.length >= MAX_LOADED ? '; the first ' + nf.format(MAX_LOADED) + ' are loaded' : '');
		var wrap = byId('tableWrap');
		if (state.playing || state.playhead >= state.duration) wrap.scrollTop = wrap.scrollHeight;
	}

	function setPlayhead(ms) {
		state.playhead = Math.max(0, Math.min(state.duration, ms));
		byId('scrub').value = String(Math.round(state.playhead));
		byId('timeText').textContent = timeText(state.playhead) + ' / ' + timeText(state.duration);
		render();
		if (channel && state.capture) channel.postMessage({ capture: state.capture.id, t: state.playhead / 1000 });
	}

	var lastFrame = 0;
	function frame(now) {
		if (!state.playing) return;
		var dt = lastFrame ? Math.min(now - lastFrame, 250) : 0;
		lastFrame = now;
		setPlayhead(state.playhead + dt * Number(byId('speed').value));
		if (state.playhead >= state.duration) { stop(); return; }
		requestAnimationFrame(frame);
	}
	function play() {
		if (!state.records.length) return;
		if (state.playhead >= state.duration) setPlayhead(0);
		state.playing = true;
		lastFrame = 0;
		byId('playBtn').textContent = 'Pause';
		requestAnimationFrame(frame);
	}
	function stop() {
		state.playing = false;
		byId('playBtn').textContent = 'Play';
	}

	byId('playBtn').addEventListener('click', function () { if (state.playing) stop(); else play(); });
	byId('endBtn').addEventListener('click', function () { stop(); setPlayhead(state.duration); });
	byId('scrub').addEventListener('input', function () { stop(); setPlayhead(Number(byId('scrub').value)); });
	byId('worldMarkers').addEventListener('click', function (e) {
		var marker = e.target.closest('[data-t]');
		if (!marker) return;
		stop();
		setPlayhead(Number(marker.dataset.t) * 1000);
	});
	byId('nameFilter').addEventListener('input', render);
	byId('sourceFilter').addEventListener('change', render);
	document.addEventListener('keydown', function (e) {
		if (e.key !== ' ' || !state.capture || /INPUT|SELECT|TEXTAREA|BUTTON/.test(document.activeElement.tagName)) return;
		e.preventDefault();
		if (state.playing) stop(); else play();
	});

	// ---- one packet ----

	function hexdump(hex) {
		var lines = [];
		for (var at = 0; at < hex.length; at += 32) {
			var chunk = hex.slice(at, at + 32), bytes = [], text = '';
			for (var i = 0; i < chunk.length; i += 2) {
				var b = parseInt(chunk.substr(i, 2), 16);
				bytes.push(chunk.substr(i, 2));
				text += b >= 32 && b < 127 ? String.fromCharCode(b) : '.';
			}
			lines.push((at / 2).toString(16).padStart(6, '0') + '  ' + bytes.join(' ').padEnd(47, ' ') + '  ' + text);
		}
		return lines.join('\n');
	}

	byId('rows').addEventListener('click', function (e) {
		var row = e.target.closest('[data-i]');
		if (row) showPacket(parseInt(row.dataset.i, 10));
	});

	function showPacket(index) {
		if (!state.capture) return;
		state.selected = index;
		render();
		var shown = byId('rows').querySelector('tr[data-i="' + index + '"]');
		if (shown) shown.scrollIntoView({ block: 'nearest' });
		api.get(url(state.capture.id) + '/packets/' + state.selected).then(function (d) {
			if (!d.record) { toast(d.error || 'Could not load the packet', 'danger'); return; }
			var r = d.record;
			byId('detail').classList.remove('d-none');
			byId('grid').classList.remove('no-detail');
			byId('detailTitle').textContent = r.name || 'Packet';
			byId('detailMeta').textContent = '#' + r.i + ' at ' + timeText(r.t) + ' · ' + (r.from || '') + ' → ' + (r.to || '') + ' · ' + r.source +
				(r.zone ? ' ' + r.zone + ':' + r.instance + (r.clone ? ':' + r.clone : '') : '') + ' · ' + nf.format(r.bytes) + ' bytes' +
				(r.account ? ' · account ' + r.account : '') + (r.character !== '0' ? ' · character ' + r.character : '') + (r.peer ? ' · ' + r.peer : '');
			byId('detailFields').textContent = r.fields ? JSON.stringify(r.fields, null, 2) : (r.gap ? r.gap + ' packets were lost here (a server buffer was full)' : 'Not decoded: no struct reads this packet (the server never sends or handles it). Its bytes are below.');
			byId('detailHex').textContent = hexdump(r.hex || '');
		}).catch(function () {});
	}
	byId('detailClose').addEventListener('click', function () {
		byId('detail').classList.add('d-none');
		byId('grid').classList.add('no-detail');
		state.selected = -1;
		render();
	});

	byId('stopBtn').addEventListener('click', function () {
		if (!state.capture) return;
		api.action('/api/inspector/packet-captures/' + state.capture.id + '/stop', {}, 'Capture stopped').then(function (d) { upsert(d.capture); }).catch(function () {});
	});
	byId('deleteBtn').addEventListener('click', function () {
		var c = state.capture;
		if (!c) return;
		Decide({ title: 'Delete capture ' + c.id, text: 'Deletes the capture and its ' + nf.format(c.received) + ' packets for good.', reasonLabel: 'Why (for the audit log)', action: 'Delete' })
			.then(function (choice) {
				if (!choice) return;
				return api.action(url(c.id) + '/delete', { reason: choice.reason }, 'Capture deleted').then(function () {
					state.live = state.live.filter(function (x) { return x.id !== c.id; });
					state.saved = state.saved.filter(function (x) { return x.id !== c.id; });
					state.capture = null;
					byId('viewer').classList.add('d-none');
					byId('emptyViewer').classList.remove('d-none');
					renderList();
				});
			}).catch(function () {});
	});

	// ---- live ----

	var reloadTimer = null;
	Live.onTopic('packet_capture', function (message) {
		upsert(message.capture);
		// New packets of the open capture: load them (at most every 2 seconds)
		if (state.capture && message.capture.id === state.capture.id && message.recent && message.recent.length && !reloadTimer) {
			reloadTimer = setTimeout(function () {
				reloadTimer = null;
				var atEnd = state.playhead >= state.duration;
				load(false).then(function () { if (atEnd && !state.playing) setPlayhead(state.duration); });
			}, 2000);
		}
	});

	onTarget();
	refreshList().then(function () {
		var match = /capture=(\d+)/.exec(window.location.hash);
		if (match) open(parseInt(match[1], 10));
	});
})();
