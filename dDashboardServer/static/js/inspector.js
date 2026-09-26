/**
 * The Game Message Inspector page: start and stop captures of a player's game messages, browse the saved ones, and open
 * one (running or saved) in the viewer. Running captures are followed live (message_capture socket topic).
 * The open capture is kept in the address (#capture=3).
 *
 * The viewer keeps every message of the open capture and draws only the rows in view (the table is virtualized), so it
 * stays quick with tens of thousands of messages. Filters, the counts by message and the search run in the browser.
 */
(function () {
	'use strict';

	var rowHeight = 26;        // px: the .msg-table row height in the template, measured once rows are drawn
	var OVERSCAN = 20;         // rows drawn above and below the visible ones
	var PAGE = 5000;           // messages per request when opening a capture
	var STATES = { waiting: ['Waiting for the player', 'warning'], capturing: ['Capturing', 'success'], ended: ['Ended', 'secondary'] };
	var nf = new Intl.NumberFormat();

	var state = {
		captures: [],         // running and recent (the side list)
		capture: null,        // summary of the open capture
		entries: [],          // its messages, in order
		bySeq: {},            // seq -> true, to drop repeats
		view: [],             // indexes into entries that pass the filters
		counts: {},           // message id -> {id, name, toServer, toClient}
		selected: -1,         // index into entries
		loading: false,
		paused: false,
		held: [],             // messages that arrived while paused
		filter: null          // the compiled view filters
	};

	function byId(id) { return document.getElementById(id); }
	function sessionUrl(id) { return '/api/inspector/sessions/' + id; }

	function bytesText(bytes) {
		if (bytes < 1024) return nf.format(bytes) + ' B';
		if (bytes < 1024 * 1024) return nf.format(Math.round(bytes / 102.4) / 10) + ' KB';
		return nf.format(Math.round(bytes / 104857.6) / 10) + ' MB';
	}

	function durationText(seconds) {
		seconds = Math.max(0, Math.round(seconds));
		return seconds >= 60 ? Math.floor(seconds / 60) + ' min ' + (seconds % 60) + ' s' : seconds + ' s';
	}

	function stateBadge(capture) {
		var s = STATES[capture.state] || [capture.state, 'secondary'];
		return fmt.badge(s[0], s[1]);
	}

	function worlds(capture) {
		if (!capture.zones || !capture.zones.length) return '<span class="text-body-secondary">-</span>';
		return capture.zones.map(function (z) {
			return '<span class="text-nowrap">' + esc(z.zoneName || ('Zone ' + z.zone)) + ' <span class="small text-body-secondary">' + esc(z.zone) + ':' + esc(z.instance) + (z.clone ? ':' + esc(z.clone) : '') + '</span></span>';
		}).join(' &rarr; ');
	}

	// ---- message names for the filters ----

	api.get('/api/inspector/messages').then(function (d) {
		if (!d.success) return;
		byId('messageNames').innerHTML = d.messages.map(function (m) { return '<option value="' + esc(m.name) + '">' + esc(m.id) + '</option>'; }).join('');
	});

	function messageList(text) {
		return text.split(/[\s,]+/).filter(Boolean);
	}

	// ---- starting and stopping ----

	byId('startForm').addEventListener('submit', function (e) {
		e.preventDefault();
		api.action('/api/inspector/captures', {
			character: byId('captureCharacter').value,
			seconds: parseInt(byId('captureSeconds').value, 10),
			toServer: byId('captureToServer').checked,
			toClient: byId('captureToClient').checked,
			only: messageList(byId('captureOnly').value),
			skip: messageList(byId('captureSkip').value)
		}, 'Capture started').then(function (d) {
			upsert(d.capture);
			open(d.capture.id);
			loadSessions();
		}).catch(function () {});
	});

	byId('stopBtn').addEventListener('click', function () {
		if (!state.capture) return;
		api.action('/api/inspector/captures/' + state.capture.id + '/stop', {}, 'Capture stopped').then(function (d) { upsert(d.capture); }).catch(function () {});
	});

	byId('deleteBtn').addEventListener('click', function () {
		var capture = state.capture;
		if (!capture) return;
		deleteSession(capture).then(function (deleted) {
			if (!deleted) return;
			closeViewer();
		});
	});

	function deleteSession(capture) {
		return Decide({
			title: 'Delete capture ' + capture.id,
			text: 'Deletes the capture of ' + capture.characterName + ' and its ' + nf.format(capture.received) + ' messages for good.',
			reasonLabel: 'Why (for the audit log)',
			action: 'Delete'
		}).then(function (choice) {
			if (!choice) return false;
			return api.action(sessionUrl(capture.id) + '/delete', { reason: choice.reason }, 'Capture deleted').then(function () {
				state.captures = state.captures.filter(function (c) { return c.id !== capture.id; });
				renderList();
				loadSessions();
				return true;
			}).catch(function () { return false; });
		});
	}

	// ---- running and recent captures (side list) ----

	function upsert(capture) {
		var index = state.captures.findIndex(function (c) { return c.id === capture.id; });
		if (index === -1) state.captures.unshift(capture); else state.captures[index] = capture;
		renderList();
		if (state.capture && capture.id === state.capture.id) {
			state.capture = capture;
			renderHeader();
		}
	}

	function timeLeft(capture) {
		return durationText(capture.endsAt - Date.now() / 1000) + ' left';
	}

	function renderList() {
		var list = byId('captureList');
		if (!state.captures.length) {
			list.innerHTML = '<div class="list-group-item text-body-secondary small">None in the last hour. Saved captures are in the list on the right.</div>';
			return;
		}
		var open = state.capture && state.capture.id;
		list.innerHTML = state.captures.map(function (c) {
			return '<a class="list-group-item list-group-item-action' + (c.id === open ? ' active' : '') + '" data-capture="' + c.id + '">' +
				'<div class="d-flex justify-content-between align-items-center"><span class="fw-semibold text-truncate">' + esc(c.characterName) + '</span>' + stateBadge(c) + '</div>' +
				'<div class="small text-body-secondary">#' + c.id + ' by ' + esc(c.startedBy) + ', ' + nf.format(c.received) + ' messages' +
				(c.state !== 'ended' ? ', ' + timeLeft(c) : '') + '</div></a>';
		}).join('');
	}

	byId('captureList').addEventListener('click', function (e) {
		var item = e.target.closest('[data-capture]');
		if (item) open(parseInt(item.dataset.capture, 10));
	});

	function refreshList() {
		return api.get('/api/inspector/captures').then(function (d) {
			if (!d.success) return;
			state.captures = d.captures;
			renderList();
		});
	}

	// ---- saved captures ----

	var sessions = { offset: 0, total: 0, order: 'started', dir: 'desc', rows: [] };

	function dayStart(input, add) {
		if (!input.value) return '';
		var parts = input.value.split('-');
		return String(Math.floor(new Date(+parts[0], +parts[1] - 1, +parts[2] + (add || 0)).getTime() / 1000));
	}

	function sessionQuery() {
		var params = new URLSearchParams();
		[['character', 'filterCharacter'], ['account', 'filterAccount'], ['staff', 'filterStaff']].forEach(function (pair) {
			var value = byId(pair[1]).value.trim();
			if (value) params.set(pair[0], value);
		});
		var since = dayStart(byId('filterSince')), until = dayStart(byId('filterUntil'), 1);
		if (since) params.set('since', since);
		if (until) params.set('until', until);
		params.set('order', sessions.order);
		params.set('dir', sessions.dir);
		params.set('offset', sessions.offset);
		params.set('limit', byId('sessionPageSize').value);
		return params.toString();
	}

	function filtered() {
		return ['filterCharacter', 'filterAccount', 'filterStaff', 'filterSince', 'filterUntil'].some(function (id) { return byId(id).value.trim(); });
	}

	var sessionRequest = 0;
	function loadSessions() {
		var request = ++sessionRequest;
		return api.get('/api/inspector/sessions?' + sessionQuery()).then(function (d) {
			if (request !== sessionRequest) return;
			if (!d.success) {
				byId('sessionRows').innerHTML = '<tr><td colspan="9" class="text-danger small">' + esc(d.error || 'Could not load the captures') + '</td></tr>';
				return;
			}
			sessions.total = d.total;
			sessions.rows = d.sessions;
			renderSessions();
		});
	}

	function renderSessions() {
		var rows = sessions.rows;
		byId('sessionRows').innerHTML = rows.length ? rows.map(function (s) {
			var end = s.state === 'ended' ? s.endedAt : Date.now() / 1000;
			return '<tr data-session="' + s.id + '">' +
				'<td class="small">' + esc(fmt.unix(s.startedAt)) + '</td>' +
				'<td>' + fmt.character(s.characterId, s.characterName) +
				(s.accountId ? '<div class="small text-body-secondary">account ' + fmt.link('/accounts/' + s.accountId, s.accountName || String(s.accountId)) + '</div>' : '') + '</td>' +
				'<td>' + esc(s.startedBy) + '</td>' +
				'<td class="text-nowrap">' + esc(durationText(end - s.startedAt)) + '</td>' +
				'<td class="small">' + worlds(s) + '</td>' +
				'<td class="text-end">' + nf.format(s.received) + (s.dropped ? ' <span class="text-warning" title="Left out by the world (too many at once)">+' + nf.format(s.dropped) + '</span>' : '') + '</td>' +
				'<td class="text-end text-nowrap">' + esc(bytesText(s.bytes)) + '</td>' +
				'<td class="small">' + (s.state === 'ended' ? esc(s.endReason) : stateBadge(s)) + '</td>' +
				'<td class="text-end text-nowrap small"><a class="btn btn-sm btn-link p-0 me-2" href="' + sessionUrl(s.id) + '/download" title="Download JSON (audited)">JSON</a>' +
				(s.state === 'ended' ? '<button type="button" class="btn btn-sm btn-link text-danger p-0" data-delete="' + s.id + '">Delete</button>' : '') + '</td></tr>';
		}).join('') : '<tr><td colspan="9" class="text-body-secondary small">No saved captures' + (filtered() ? ' match these filters' : ' yet') + '.</td></tr>';
		var limit = parseInt(byId('sessionPageSize').value, 10);
		byId('sessionInfo').textContent = sessions.total ? (nf.format(sessions.offset + 1) + '–' + nf.format(sessions.offset + rows.length) + ' of ' + nf.format(sessions.total)) : '';
		byId('sessionPrev').disabled = sessions.offset === 0;
		byId('sessionNext').disabled = sessions.offset + limit >= sessions.total;
		document.querySelectorAll('.sessions-table th[data-sort]').forEach(function (th) {
			th.classList.toggle('sorted-asc', th.dataset.sort === sessions.order && sessions.dir === 'asc');
			th.classList.toggle('sorted-desc', th.dataset.sort === sessions.order && sessions.dir === 'desc');
		});
	}

	byId('sessionRows').addEventListener('click', function (e) {
		if (e.target.closest('a')) return;
		var del = e.target.closest('[data-delete]');
		if (del) {
			var s = sessions.rows.filter(function (r) { return r.id === parseInt(del.dataset.delete, 10); })[0];
			if (s) deleteSession(s);
			return;
		}
		var row = e.target.closest('[data-session]');
		if (row) open(parseInt(row.dataset.session, 10));
	});

	document.querySelectorAll('.sessions-table th[data-sort]').forEach(function (th) {
		th.addEventListener('click', function () {
			if (sessions.order === th.dataset.sort) sessions.dir = sessions.dir === 'asc' ? 'desc' : 'asc';
			else { sessions.order = th.dataset.sort; sessions.dir = th.dataset.sort === 'character' || th.dataset.sort === 'started_by' ? 'asc' : 'desc'; }
			sessions.offset = 0;
			loadSessions();
		});
	});

	var reloadSessions = Live.throttle(function () { sessions.offset = 0; loadSessions(); }, 300);
	['filterCharacter', 'filterAccount', 'filterStaff'].forEach(function (id) { byId(id).addEventListener('input', reloadSessions); });
	['filterSince', 'filterUntil', 'sessionPageSize'].forEach(function (id) { byId(id).addEventListener('change', reloadSessions); });
	byId('sessionFilters').addEventListener('submit', function (e) { e.preventDefault(); reloadSessions(); });
	byId('filterClear').addEventListener('click', function () {
		['filterCharacter', 'filterAccount', 'filterStaff', 'filterSince', 'filterUntil'].forEach(function (id) { byId(id).value = ''; });
		reloadSessions();
	});
	byId('sessionPrev').addEventListener('click', function () {
		sessions.offset = Math.max(0, sessions.offset - parseInt(byId('sessionPageSize').value, 10));
		loadSessions();
	});
	byId('sessionNext').addEventListener('click', function () {
		sessions.offset += parseInt(byId('sessionPageSize').value, 10);
		loadSessions();
	});

	// ---- tabs ----

	function showTab(viewer) {
		byId('inspectorLayout').classList.toggle('viewing', viewer);
		byId('sessionsPane').classList.toggle('d-none', viewer);
		byId('viewerPane').classList.toggle('d-none', !viewer);
		byId('sessionsTab').classList.toggle('active', !viewer);
		byId('viewerTab').classList.toggle('active', viewer);
		byId('sessionsTab').setAttribute('aria-selected', String(!viewer));
		byId('viewerTab').setAttribute('aria-selected', String(viewer));
		if (viewer) drawRows(); else loadSessions();
	}
	byId('sessionsTab').addEventListener('click', function () { showTab(false); history.replaceState(null, '', '#'); });
	byId('viewerTab').addEventListener('click', function () {
		if (!state.capture) return;
		showTab(true);
		history.replaceState(null, '', '#capture=' + state.capture.id);
	});

	function closeViewer() {
		state.capture = null;
		state.entries = [];
		state.view = [];
		byId('viewerTab').disabled = true;
		byId('viewerTab').textContent = 'Viewer';
		renderList();
		showTab(false);
		history.replaceState(null, '', '#');
	}

	// ---- the open capture ----

	var openRequest = 0;

	function open(id) {
		var request = ++openRequest;
		state.capture = null;
		state.entries = [];
		state.bySeq = {};
		state.view = [];
		state.counts = {};
		state.selected = -1;
		state.held = [];
		state.paused = false;
		state.loading = true;
		history.replaceState(null, '', '#capture=' + id);
		hideDetail();
		byId('viewerTab').disabled = false;
		byId('viewerTab').textContent = 'Capture ' + id;
		byId('captureTitle').textContent = 'Loading capture ' + id + '…';
		byId('captureSubtitle').textContent = '';
		showTab(true);
		renderList();
		loadPage(id, 0, request);
	}

	// Pages of messages until all are here
	function loadPage(id, after, request) {
		api.get(sessionUrl(id) + '/entries?after=' + after + '&limit=' + PAGE).then(function (d) {
			if (request !== openRequest) return;
			if (!d.success) {
				state.loading = false;
				byId('captureTitle').textContent = d.error || 'Could not open the capture';
				return;
			}
			state.capture = d.capture;
			renderHeader();
			renderList();
			addEntries(d.entries);
			if (d.more && d.entries.length) {
				byId('rowInfo').textContent = 'Loading… ' + nf.format(state.entries.length) + ' of ' + nf.format(d.capture.received);
				loadPage(id, d.entries[d.entries.length - 1].seq, request);
			} else {
				state.loading = false;
				updateInfo();
			}
		}).catch(function () { state.loading = false; });
	}

	function renderHeader() {
		var capture = state.capture;
		if (!capture) return;
		byId('captureTitle').innerHTML = fmt.character(capture.characterId, capture.characterName) + ' <span class="text-body-secondary fs-6">capture ' + esc(capture.id) + '</span> ' + stateBadge(capture);
		var parts = [];
		if (capture.state === 'waiting') parts.push('Not in a world right now (logged out or changing zones); it carries on when they are');
		if (capture.zones && capture.zones.length) parts.push(worlds(capture));
		if (capture.state !== 'ended') parts.push(esc(timeLeft(capture)));
		else parts.push(esc(capture.endReason) + ' after ' + esc(durationText(capture.endedAt - capture.startedAt)));
		parts.push(nf.format(capture.received) + ' messages, ' + esc(bytesText(capture.bytes)));
		if (capture.dropped) parts.push('<span class="text-warning">' + nf.format(capture.dropped) + ' left out (too many at once)</span>');
		var filters = [];
		if (!capture.toServer) filters.push('only sent to the client');
		if (!capture.toClient) filters.push('only sent by the client');
		if (capture.only.length) filters.push('only ' + capture.only.map(esc).join(', '));
		if (capture.skip.length) filters.push('not ' + capture.skip.map(esc).join(', '));
		if (filters.length) parts.push(filters.join('; '));
		parts.push('started by ' + esc(capture.startedBy) + ' ' + esc(fmt.unix(capture.startedAt)));
		if (capture.accountId) parts.push('account ' + fmt.link('/accounts/' + capture.accountId, capture.accountName || String(capture.accountId)));
		byId('captureSubtitle').innerHTML = parts.join(' &middot; ');
		var live = capture.state !== 'ended';
		byId('stopBtn').classList.toggle('d-none', !live);
		byId('pauseBtn').classList.toggle('d-none', !live);
		byId('deleteBtn').classList.toggle('d-none', live);
		byId('downloadBtn').href = sessionUrl(capture.id) + '/download';
	}

	// ---- filters ----

	function compileFilter() {
		var include = [], exclude = [];
		byId('viewFilter').value.split(',').forEach(function (term) {
			term = term.trim().toUpperCase();
			if (!term) return;
			var not = term.charAt(0) === '!';
			if (not) term = term.slice(1).trim();
			if (!term) return;
			var exact = term.charAt(0) === '=';
			if (exact) term = term.slice(1);
			var test = /^\d+$/.test(term) ? (function (id) { return function (e) { return e.id === id; }; })(parseInt(term, 10))
				: exact ? function (e) { return e.name === term; } : function (e) { return e.name.indexOf(term) !== -1; };
			(not ? exclude : include).push(test);
		});
		var search = byId('viewSearch').value.trim().toLowerCase();
		return {
			dir: byId('viewDirection').value,
			object: byId('viewObject').value.trim(),
			decoded: byId('viewDecoded').checked,
			search: search,
			hexSearch: search.replace(/\s+/g, ''),
			include: include,
			exclude: exclude,
			active: !!(byId('viewDirection').value || byId('viewObject').value.trim() || byId('viewDecoded').checked || search || include.length || exclude.length)
		};
	}

	function fieldsText(entry) {
		if (entry._text === undefined) entry._text = entry.fields ? JSON.stringify(entry.fields).toLowerCase() : '';
		return entry._text;
	}

	function passes(entry, f) {
		if (f.dir && entry.dir !== f.dir) return false;
		if (f.object && entry.object !== f.object) return false;
		if (f.decoded && !entry.fields) return false;
		if (f.include.length && !f.include.some(function (t) { return t(entry); })) return false;
		if (f.exclude.some(function (t) { return t(entry); })) return false;
		if (f.search && fieldsText(entry).indexOf(f.search) === -1 && (!/^[0-9a-f]+$/.test(f.hexSearch) || entry.hex.indexOf(f.hexSearch) === -1)) return false;
		return true;
	}

	function refilter() {
		state.filter = compileFilter();
		var view = [];
		for (var i = 0; i < state.entries.length; i++) if (passes(state.entries[i], state.filter)) view.push(i);
		state.view = view;
		byId('clearView').classList.toggle('d-none', !state.filter.active);
		renderTypes();
		drawRows(true);
		updateInfo();
	}

	['viewFilter', 'viewObject', 'viewSearch'].forEach(function (id) { byId(id).addEventListener('input', Live.throttle(refilter, 200)); });
	['viewDirection', 'viewDecoded'].forEach(function (id) { byId(id).addEventListener('change', refilter); });
	byId('viewRelative').addEventListener('change', function () { drawRows(true); if (state.selected !== -1) showDetail(state.selected); });
	byId('viewFollow').addEventListener('change', function () { if (byId('viewFollow').checked) scrollToEnd(); });
	byId('clearView').addEventListener('click', function (e) {
		e.preventDefault();
		['viewFilter', 'viewObject', 'viewSearch', 'viewDirection'].forEach(function (id) { byId(id).value = ''; });
		byId('viewDecoded').checked = false;
		refilter();
	});

	// ---- counts by message ----

	function count(entry) {
		var c = state.counts[entry.id] || (state.counts[entry.id] = { id: entry.id, name: entry.name, toServer: 0, toClient: 0 });
		if (entry.dir === 'to_server') c.toServer++; else c.toClient++;
	}

	function renderTypes() {
		var on = byId('viewTypes').checked;
		byId('typesCard').classList.toggle('d-none', !on);
		if (!on) return;
		var exact = byId('viewFilter').value.trim().toUpperCase();
		var rows = Object.keys(state.counts).map(function (k) { return state.counts[k]; })
			.sort(function (a, b) { return (b.toServer + b.toClient) - (a.toServer + a.toClient) || a.id - b.id; });
		byId('typeRows').innerHTML = rows.length ? rows.map(function (c) {
			return '<tr data-name="' + esc(c.name) + '"' + (exact === '=' + c.name ? ' class="active-filter"' : '') + '><td class="fw-semibold">' + esc(c.name) + '</td><td class="text-end text-body-secondary">' + c.id + '</td>' +
				'<td class="text-end">' + nf.format(c.toServer) + '</td><td class="text-end">' + nf.format(c.toClient) + '</td><td class="text-end">' + nf.format(c.toServer + c.toClient) + '</td></tr>';
		}).join('') : '<tr><td colspan="5" class="text-body-secondary small">No messages yet.</td></tr>';
	}
	var renderTypesSoon = Live.throttle(renderTypes, 500);

	byId('viewTypes').addEventListener('change', renderTypes);
	byId('typeRows').addEventListener('click', function (e) {
		var row = e.target.closest('[data-name]');
		if (!row) return;
		var value = '=' + row.dataset.name;
		byId('viewFilter').value = byId('viewFilter').value.trim().toUpperCase() === value ? '' : value;
		refilter();
	});

	// ---- the message table (virtualized) ----

	function timeText(entry) {
		if (byId('viewRelative').checked && state.capture) {
			var ms = Math.max(0, entry.time - state.capture.startedAt * 1000);
			var m = Math.floor(ms / 60000), s = Math.floor(ms / 1000) % 60;
			return '+' + m + ':' + String(s).padStart(2, '0') + '.' + String(ms % 1000).padStart(3, '0');
		}
		var t = new Date(entry.time);
		return t.toLocaleTimeString() + '.' + String(t.getMilliseconds()).padStart(3, '0');
	}

	function fieldsSummary(fields) {
		if (!fields) return '';
		return Object.keys(fields).map(function (k) {
			var v = fields[k];
			return k + '=' + (typeof v === 'object' ? JSON.stringify(v) : v);
		}).join(' ');
	}

	function rowHtml(index, previous) {
		var entry = state.entries[index];
		var classes = [];
		if (index === state.selected) classes.push('selected');
		if (entry.gap) classes.push('has-gap');
		var moved = previous && (previous.zone !== entry.zone || previous.instance !== entry.instance || previous.clone !== entry.clone);
		if (moved) classes.push('zone-change');
		var arrow = entry.dir === 'to_server'
			? '<span class="badge text-bg-info" title="Sent by the client">&rarr; server</span>'
			: '<span class="badge text-bg-primary" title="Sent to the client">&larr; client</span>';
		var title = [];
		if (entry.gap) title.push(nf.format(entry.gap) + ' message(s) left out just before this one: too many at once');
		if (moved) title.push('Now in zone ' + entry.zone + ', instance ' + entry.instance + (entry.clone ? ', clone ' + entry.clone : ''));
		return '<tr data-index="' + index + '"' + (classes.length ? ' class="' + classes.join(' ') + '"' : '') + (title.length ? ' title="' + esc(title.join('. ')) + '"' : '') + '>' +
			'<td class="text-body-secondary">' + entry.seq + (entry.gap ? ' <span class="text-warning">+' + nf.format(entry.gap) + '</span>' : '') + '</td>' +
			'<td>' + esc(timeText(entry)) + '</td>' +
			'<td>' + arrow + '</td>' +
			'<td><span class="fw-semibold">' + esc(entry.name) + '</span> <span class="text-body-secondary">' + entry.id + '</span></td>' +
			'<td class="font-monospace"><a href="#" data-object="' + esc(entry.object) + '" title="Show only messages to this object">' + esc(entry.object) + '</a></td>' +
			'<td class="text-end">' + nf.format(entry.bytes) + '</td>' +
			'<td class="font-monospace">' + (entry.fields ? esc(fieldsSummary(entry.fields)) : '<span class="text-body-secondary">' + esc(entry.hex.slice(0, 48)) + (entry.hex.length > 48 ? '…' : '') + '</span>') + '</td></tr>';
	}

	var drawn = { first: -1, last: -1, length: -1 };

	function drawRows(force) {
		if (byId('viewerPane').classList.contains('d-none')) return;
		var wrap = byId('tableWrap');
		var total = state.view.length;
		var height = wrap.clientHeight || 400;
		var first = Math.max(0, Math.floor(wrap.scrollTop / rowHeight) - OVERSCAN);
		var last = Math.min(total, Math.ceil((wrap.scrollTop + height) / rowHeight) + OVERSCAN);
		if (!force && first === drawn.first && last === drawn.last && total === drawn.length) return;
		drawn = { first: first, last: last, length: total };
		var html = first > 0 ? '<tr class="spacer" style="height:' + (first * rowHeight) + 'px"><td colspan="7"></td></tr>' : '';
		for (var i = first; i < last; i++) html += rowHtml(state.view[i], i > 0 ? state.entries[state.view[i - 1]] : null);
		if (last < total) html += '<tr class="spacer" style="height:' + ((total - last) * rowHeight) + 'px"><td colspan="7"></td></tr>';
		if (!total) html = '<tr><td colspan="7" class="text-body-secondary small">' + (state.loading ? 'Loading…' : state.entries.length ? 'No messages match the filters.' : 'No messages yet.') + '</td></tr>';
		byId('messageRows').innerHTML = html;
		// The spacers must match the real row height (fonts and zoom can change it)
		var row = byId('messageRows').querySelector('tr[data-index]');
		var measured = row ? row.getBoundingClientRect().height : 0;
		if (measured && Math.abs(measured - rowHeight) > 0.5) {
			rowHeight = measured;
			drawRows(true);
		}
	}

	byId('tableWrap').addEventListener('scroll', function () {
		drawRows(false);
		// Scrolling up stops following; back at the bottom follows again
		var wrap = byId('tableWrap');
		var atEnd = wrap.scrollTop + wrap.clientHeight >= wrap.scrollHeight - rowHeight;
		if (state.capture && state.capture.state !== 'ended' && byId('viewFollow').checked !== atEnd && !programmaticScroll) {
			byId('viewFollow').checked = atEnd;
		}
		programmaticScroll = false;
	});
	window.addEventListener('resize', Live.throttle(function () { drawRows(true); }, 200));

	var programmaticScroll = false;
	function scrollToEnd() {
		var wrap = byId('tableWrap');
		programmaticScroll = true;
		wrap.scrollTop = wrap.scrollHeight;
		drawRows(false);
	}

	function updateInfo() {
		var text = nf.format(state.view.length) + ' of ' + nf.format(state.entries.length) + ' shown';
		if (state.loading && state.capture) text = 'Loading… ' + nf.format(state.entries.length) + ' of ' + nf.format(state.capture.received);
		byId('rowInfo').textContent = text;
		var resume = byId('resumeLink');
		resume.classList.toggle('d-none', !state.paused);
		resume.textContent = state.paused ? 'Paused: ' + nf.format(state.held.length) + ' new. Resume' : '';
	}

	function addEntries(entries) {
		if (!entries.length) return;
		var f = state.filter || (state.filter = compileFilter());
		var outOfOrder = false;
		var last = state.entries.length ? state.entries[state.entries.length - 1].seq : 0;
		entries.forEach(function (entry) {
			if (state.bySeq[entry.seq]) return;
			state.bySeq[entry.seq] = true;
			if (entry.seq < last) outOfOrder = true;
			last = Math.max(last, entry.seq);
			state.entries.push(entry);
			count(entry);
			if (!outOfOrder && passes(entry, f)) state.view.push(state.entries.length - 1);
		});
		if (outOfOrder) {
			// Pushed messages can arrive before the ones fetched when the capture was opened: put them in order
			var selected = state.selected !== -1 ? state.entries[state.selected] : null;
			state.entries.sort(function (a, b) { return a.seq - b.seq; });
			state.selected = selected ? state.entries.indexOf(selected) : -1;
			refilter();
		} else {
			drawRows(true);
			renderTypesSoon();
		}
		updateInfo();
		if (byId('viewFollow').checked && state.capture && state.capture.state !== 'ended') scrollToEnd();
	}

	// ---- pause ----

	function setPaused(paused) {
		state.paused = paused;
		byId('pauseBtn').textContent = paused ? 'Resume' : 'Pause';
		byId('pauseBtn').classList.toggle('active', paused);
		if (!paused && state.held.length) {
			var held = state.held;
			state.held = [];
			addEntries(held);
		}
		updateInfo();
	}
	byId('pauseBtn').addEventListener('click', function () { setPaused(!state.paused); });
	byId('resumeLink').addEventListener('click', function (e) { e.preventDefault(); setPaused(false); });

	// ---- one message ----

	function hexDump(hex) {
		var lines = [], count = hex.length / 2;
		for (var offset = 0; offset < count; offset += 16) {
			var bytes = '', text = '';
			for (var i = offset; i < offset + 16; i++) {
				if (i < count) {
					var pair = hex.substr(i * 2, 2), b = parseInt(pair, 16);
					bytes += '<span class="b" data-i="' + i + '">' + pair + '</span>' + (i % 8 === 7 ? '  ' : ' ');
					text += '<span class="a" data-i="' + i + '">' + (b >= 32 && b < 127 ? esc(String.fromCharCode(b)) : '.') + '</span>';
				} else {
					bytes += '   ' + (i % 8 === 7 ? ' ' : '');
				}
			}
			lines.push('<span class="off">' + offset.toString(16).padStart(4, '0') + '</span>  ' + bytes + ' ' + text);
		}
		return lines.join('\n');
	}

	function valueHtml(v) {
		if (v !== null && typeof v === 'object') {
			// Short values (positions, rotations) on one line, longer ones laid out
			var line = JSON.stringify(v);
			return line.length <= 80 ? esc(line) : '<pre class="mb-0 small">' + esc(JSON.stringify(v, null, 1)) + '</pre>';
		}
		return esc(v);
	}

	function showDetail(index) {
		var entry = state.entries[index];
		if (!entry) return;
		state.selected = index;
		byId('viewerGrid').classList.remove('no-detail');
		byId('detailCard').classList.remove('d-none');
		byId('detailTitle').textContent = '#' + entry.seq + ' ' + entry.name + ' (' + entry.id + ')';
		var at = new Date(entry.time);
		byId('detailMeta').innerHTML = esc(entry.dir === 'to_server' ? 'Sent by the client' : 'Sent to the client') + ' at ' + esc(at.toLocaleString() + '.' + String(at.getMilliseconds()).padStart(3, '0')) +
			(state.capture ? ' (' + esc(timeTextRelative(entry)) + ' into the capture)' : '') + ', to object <span class="font-monospace">' + esc(entry.object) + '</span>, ' +
			nf.format(entry.bits) + ' bits' + (entry.truncated ? ' <span class="text-warning">(only the first ' + nf.format(entry.hex.length / 2) + ' bytes were kept)</span>' : '') +
			(entry.zone ? ', in zone ' + esc(entry.zone) + ' instance ' + esc(entry.instance) + (entry.clone ? ' clone ' + esc(entry.clone) : '') : '') +
			(entry.gap ? '. <span class="text-warning">' + nf.format(entry.gap) + ' message(s) were left out just before it.</span>' : '');
		var fields = byId('detailFields');
		if (entry.fields) {
			fields.innerHTML = '<table class="table table-sm mb-0 detail-fields"><tbody>' + Object.keys(entry.fields).map(function (k) {
				return '<tr><th class="fw-normal text-body-secondary">' + esc(k) + '</th><td class="font-monospace text-break">' + valueHtml(entry.fields[k]) + '</td></tr>';
			}).join('') + '</tbody></table>';
		} else {
			fields.innerHTML = '<div class="small text-body-secondary">The server reads this message without a typed struct, so only its bytes are shown.</div>';
		}
		byId('detailHex').innerHTML = entry.hex ? hexDump(entry.hex) : '(no bytes)';
		drawRows(true);
	}

	function timeTextRelative(entry) {
		var ms = Math.max(0, entry.time - state.capture.startedAt * 1000);
		return (ms / 1000).toFixed(3) + ' s';
	}

	function hideDetail() {
		state.selected = -1;
		byId('detailCard').classList.add('d-none');
		byId('viewerGrid').classList.add('no-detail');
	}

	// Highlight a byte and its character together
	byId('detailHex').addEventListener('mouseover', function (e) {
		var el = e.target.closest('[data-i]');
		byId('detailHex').querySelectorAll('.hl').forEach(function (h) { h.classList.remove('hl'); });
		if (el) byId('detailHex').querySelectorAll('[data-i="' + el.dataset.i + '"]').forEach(function (h) { h.classList.add('hl'); });
	});
	byId('detailHex').addEventListener('mouseleave', function () {
		byId('detailHex').querySelectorAll('.hl').forEach(function (h) { h.classList.remove('hl'); });
	});

	function copy(text, what) {
		if (!navigator.clipboard) return toast('Copying needs a secure (https) page', 'warning');
		navigator.clipboard.writeText(text).then(function () { toast(what + ' copied', 'success'); }, function () { toast('Could not copy', 'danger'); });
	}
	byId('copyHex').addEventListener('click', function () {
		var entry = state.entries[state.selected];
		if (entry) copy(entry.hex, 'Hex');
	});
	byId('copyJson').addEventListener('click', function () {
		var entry = state.entries[state.selected];
		if (!entry) return;
		var plain = {};
		Object.keys(entry).forEach(function (k) { if (k.charAt(0) !== '_') plain[k] = entry[k]; });
		copy(JSON.stringify(plain, null, 1), 'Message');
	});

	byId('messageRows').addEventListener('click', function (e) {
		var object = e.target.closest('[data-object]');
		if (object) {
			e.preventDefault();
			byId('viewObject').value = byId('viewObject').value === object.dataset.object ? '' : object.dataset.object;
			refilter();
			return;
		}
		var row = e.target.closest('tr[data-index]');
		if (!row) return;
		showDetail(parseInt(row.dataset.index, 10));
		byId('detailCard').scrollIntoView({ block: 'nearest' });
	});
	byId('detailClose').addEventListener('click', function () { hideDetail(); drawRows(true); });

	// Up and down move through the shown messages
	byId('tableWrap').addEventListener('keydown', function (e) {
		if (e.key !== 'ArrowDown' && e.key !== 'ArrowUp' || !state.view.length) return;
		e.preventDefault();
		var at = state.view.indexOf(state.selected);
		at = at === -1 ? (e.key === 'ArrowDown' ? 0 : state.view.length - 1) : Math.max(0, Math.min(state.view.length - 1, at + (e.key === 'ArrowDown' ? 1 : -1)));
		byId('viewFollow').checked = false;
		// Rows sit under the sticky header: keep the selected one between it and the bottom
		var wrap = byId('tableWrap'), top = at * rowHeight, header = wrap.querySelector('thead').getBoundingClientRect().height;
		if (top < wrap.scrollTop) wrap.scrollTop = top;
		else if (header + top + rowHeight > wrap.scrollTop + wrap.clientHeight) wrap.scrollTop = header + top + rowHeight - wrap.clientHeight;
		showDetail(state.view[at]);
	});

	// ---- live ----

	Live.onTopic('message_capture', function (message) {
		upsert(message.capture);
		if (!state.capture || message.capture.id !== state.capture.id || !message.entries.length) return;
		if (state.paused) {
			Array.prototype.push.apply(state.held, message.entries);
			updateInfo();
			return;
		}
		addEntries(message.entries);
	});
	// Time left, once a second
	setInterval(function () {
		if (state.captures.some(function (c) { return c.state !== 'ended'; })) {
			renderList();
			if (state.capture && state.capture.state !== 'ended') renderHeader();
		}
	}, 1000);

	refreshList().then(function () {
		var match = /capture=(\d+)/.exec(window.location.hash);
		if (match) open(parseInt(match[1], 10)); else loadSessions();
	});
})();
