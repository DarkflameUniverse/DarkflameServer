/**
 * Live updates over a single WebSocket.
 *
 * The server pushes three topics:
 *  - dashboard_update  : server status and totals
 *  - table_changed     : {table, id?} - refetch that table through the normal API
 *  - moderation_counts : moderation queue sizes for the sidebar badges (GM 3+)
 *
 * Pages call Live.watchTable(name, dataTable), Live.on(name, callback) or Live.refreshPage(names, id) to react to
 * changes. Game servers report their writes as they happen (world -> master -> dashboard), and the dashboard also
 * diffs a database snapshot for writers outside the game. After a reconnect every watcher is told to resync, since
 * changes may have been missed while the socket was down.
 */
(function () {
	'use strict';

	var MAX_BACKOFF_MS = 30000;
	var RELOAD_THROTTLE_MS = 1500;   // busy servers report changes constantly; reload tables at most this often
	var PAGE_REFRESH_MIN_MS = 3000;

	var ws = null;
	var attempts = 0;
	var tableWatchers = {};  // table name -> [callback]
	var statusWatchers = [];
	var actionWaiters = {};  // requestId -> callback for actions started from this tab
	var topicWatchers = {};  // extra topics a page asked for (e.g. player_positions) -> [callback]
	var wasConnected = false;

	function setIndicator(text, cls) {
		var el = document.getElementById('live-indicator');
		if (!el) return;
		el.textContent = text;
		el.className = cls;
	}

	function subscribe(topic) {
		if (ws && ws.readyState === WebSocket.OPEN) ws.send(JSON.stringify({ event: 'subscribe', subscription: topic }));
	}

	function notifyTable(table, data) {
		(tableWatchers[table] || []).forEach(function (cb) {
			try { cb(data); } catch (e) { console.error(e); }
		});
	}

	function handleMessage(data) {
		switch (data.event) {
			case 'dashboard_update':
				statusWatchers.forEach(function (cb) { cb(data); });
				break;
			case 'table_changed':
				notifyTable(data.table, data);
				break;
			case 'moderation_counts':
				updateBadges(data);
				break;
			case 'action_result':
				resolveAction(data);
				break;
			case 'session_ended':
				// Signed out everywhere, banned or no longer allowed on the dashboard
				api.get('/api/auth/me').then(function (me) { if (!me.valid) window.location.href = '/login'; }).catch(function () {});
				break;
			default:
				(topicWatchers[data.event] || []).forEach(function (cb) {
					try { cb(data); } catch (e) { console.error(e); }
				});
		}
		if (data.event === 'dashboard_update') showRestart(data.restart);
	}

	function connect() {
		var protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
		try {
			ws = new WebSocket(protocol + '//' + window.location.host + '/ws');
		} catch (e) {
			scheduleReconnect();
			return;
		}

		ws.onopen = function () {
			attempts = 0;
			setIndicator('live', 'text-success');
			if (wasConnected) {
				// Anything could have changed while we were away
				Object.keys(tableWatchers).forEach(function (table) { notifyTable(table, { table: table, resync: true }); });
			}
			wasConnected = true;
			subscribe('dashboard_update');
			if (DASH.gmLevel >= 1) subscribe('table_changed');
			if (DASH.can('moderate_names')) subscribe('moderation_counts');
			subscribe('action_result');
			Object.keys(topicWatchers).forEach(subscribe);
		};
		ws.onmessage = function (event) {
			try { handleMessage(JSON.parse(event.data)); } catch (e) { /* ignore malformed frames */ }
		};
		ws.onclose = function () {
			ws = null;
			setIndicator('reconnecting…', 'text-warning');
			scheduleReconnect();
		};
		ws.onerror = function () { /* onclose follows */ };
	}

	function scheduleReconnect() {
		attempts++;
		var delay = Math.min(MAX_BACKOFF_MS, 1000 * Math.pow(2, attempts - 1));
		// Our session may have expired; check before hammering the socket
		if (attempts > 3) {
			api.get('/api/auth/me').then(function (me) { if (!me.valid) window.location.href = '/login'; }).catch(function () {});
		}
		setTimeout(connect, delay);
	}

	function resolveAction(result) {
		var cb = actionWaiters[result.requestId];
		if (!cb) return;
		delete actionWaiters[result.requestId];
		cb(result);
	}

	function updateBadges(counts) {
		var map = {
			'moderation': (counts.pendingNames || 0) + (DASH.can('moderate_pet_names') ? counts.pendingPetNames || 0 : 0) + (DASH.can('moderate_properties') ? counts.pendingProperties || 0 : 0),
			'pet_names': counts.pendingPetNames || 0,
			'bug_reports': counts.unresolvedBugReports || 0,
			'reports': counts.openEconomyFlags || 0
		};
		Object.keys(map).forEach(function (key) {
			var el = document.querySelector('[data-badge="' + key + '"]');
			if (!el) return;
			el.textContent = map[key];
			el.classList.toggle('d-none', !map[key]);
		});
	}

	// Run fn at most once per interval, always including the last call (leading and trailing)
	function throttle(fn, interval) {
		var last = 0, timer = null;
		return function () {
			var wait = last + interval - Date.now();
			if (wait <= 0 && !timer) {
				last = Date.now();
				fn();
			} else if (!timer) {
				timer = setTimeout(function () { timer = null; last = Date.now(); fn(); }, Math.max(wait, 0));
			}
		};
	}

	// Whether the user is in the middle of something a page reload would throw away
	function isBusy() {
		var active = document.activeElement;
		if (active && active !== document.body && active.matches('input:not([type=checkbox]):not([type=radio]), textarea, select, [contenteditable]')) return true;
		if (document.querySelector('.modal.show, .dropdown-menu.show, .offcanvas.show')) return true;
		var selection = window.getSelection && window.getSelection();
		return !!(selection && String(selection).length);
	}

	var SCROLL_KEY = 'live-scroll:' + window.location.pathname + window.location.search;
	try {
		var savedScroll = sessionStorage.getItem(SCROLL_KEY);
		if (savedScroll !== null) {
			sessionStorage.removeItem(SCROLL_KEY);
			window.addEventListener('load', function () { window.scrollTo(0, parseInt(savedScroll, 10) || 0); });
		}
	} catch (e) { /* storage unavailable */ }

	function reloadKeepingScroll() {
		try { sessionStorage.setItem(SCROLL_KEY, String(window.scrollY)); } catch (e) { /* storage unavailable */ }
		window.location.reload();
	}

	function showStaleBanner() {
		if (document.getElementById('live-stale')) return;
		var banner = document.createElement('div');
		banner.id = 'live-stale';
		banner.className = 'alert alert-info d-flex align-items-center gap-3 position-fixed bottom-0 start-50 translate-middle-x mb-3 shadow';
		banner.setAttribute('role', 'status');
		banner.style.zIndex = 1090;
		var text = document.createElement('span');
		text.textContent = 'This changed while you were editing.';
		var button = document.createElement('button');
		button.type = 'button';
		button.className = 'btn btn-sm btn-primary';
		button.textContent = 'Refresh';
		button.addEventListener('click', reloadKeepingScroll);
		banner.append(text, button);
		document.body.appendChild(banner);
	}

	// A countdown on every page while a server restart is scheduled
	var restartTimer = null;
	function showRestart(restart) {
		var banner = document.getElementById('restart-banner');
		clearInterval(restartTimer);
		if (!restart || !restart.at) {
			if (banner) banner.remove();
			return;
		}
		if (!banner) {
			banner = document.createElement('div');
			banner.id = 'restart-banner';
			banner.className = 'alert alert-warning d-flex align-items-center gap-2 mb-3';
			banner.setAttribute('role', 'status');
			var main = document.querySelector('main') || document.body;
			main.insertBefore(banner, main.firstChild);
		}
		function tick() {
			var left = Math.max(0, Math.round(restart.at - Date.now() / 1000));
			var text = left >= 60 ? Math.floor(left / 60) + ' min ' + (left % 60) + ' s' : left + ' s';
			banner.textContent = 'Server restart in ' + text + (restart.reason ? ': ' + restart.reason : '') + (restart.by ? ' (scheduled by ' + restart.by + ')' : '');
		}
		tick();
		restartTimer = setInterval(tick, 1000);
	}

	window.Live = {
		// Receive a socket topic this page needs (subscribed now and after every reconnect)
		onTopic: function (topic, cb) {
			var first = !topicWatchers[topic];
			(topicWatchers[topic] = topicWatchers[topic] || []).push(cb);
			if (first) subscribe(topic);
		},
		// Call cb({table, id}) whenever the named table changes
		on: function (table, cb) {
			(tableWatchers[table] = tableWatchers[table] || []).push(cb);
		},
		// Reload a DataTable (keeping its page) whenever the named table changes
		watchTable: function (table, dataTable) {
			Live.on(table, throttle(function () { dataTable.ajax.reload(null, false); }, RELOAD_THROTTLE_MS));
		},
		/**
		 * For server-rendered pages about one row: reload the page when that row changes in any of the given tables
		 * (or any row, with anyRow). If the user is typing or has a dialog open, offer a refresh instead.
		 */
		refreshPage: function (tables, id, options) {
			options = options || {};
			var refresh = throttle(function () { if (isBusy()) showStaleBanner(); else reloadKeepingScroll(); }, PAGE_REFRESH_MIN_MS);
			[].concat(tables).forEach(function (table) {
				Live.on(table, function (e) {
					if (e.resync || options.anyRow || String(e.id) === String(id)) refresh();
				});
			});
		},
		throttle: throttle,
		onStatus: function (cb) { statusWatchers.push(cb); },
		/**
		 * Wait for background work (a scan, a search) and fetch its result, including its data, which only the account
		 * that started it can read. Resolves with {success, message, data}. Keeps checking with a growing interval
		 * in case the socket is down.
		 */
		waitForResult: function (requestId) {
			return new Promise(function (resolve) {
				var done = false, delay = 1000;
				function fetchResult() {
					if (done) return;
					api.get('/api/actions/' + requestId).then(function (r) {
						if (r.status === 'done') { done = true; delete actionWaiters[requestId]; resolve(r); } else schedule();
					}).catch(schedule);
				}
				function schedule() {
					if (done) return;
					setTimeout(fetchResult, delay);
					delay = Math.min(delay * 1.5, 10000);
				}
				actionWaiters[requestId] = fetchResult;
				schedule();
			});
		},
		/**
		 * Wait for the result of an asynchronous player action (kick, rescue, ...). Falls back to polling
		 * /api/actions/:id if the socket is down. Resolves with {success, message, affected}.
		 */
		waitForAction: function (requestId) {
			return new Promise(function (resolve) {
				var done = false;
				function finish(result) { if (!done) { done = true; resolve(result); } }
				actionWaiters[requestId] = finish;
				var polls = 0;
				(function poll() {
					if (done || polls++ > 15) return;
					setTimeout(function () {
						if (done) return;
						api.get('/api/actions/' + requestId).then(function (r) {
							if (r.status === 'done') { delete actionWaiters[requestId]; finish(r); } else poll();
						}).catch(poll);
					}, 1000);
				})();
			});
		}
	};

	// Logout clears the HttpOnly cookie server-side
	document.addEventListener('click', function (e) {
		if (!e.target.closest('#logoutBtn')) return;
		e.preventDefault();
		api.post('/api/auth/logout').finally(function () { window.location.href = '/login'; });
	});

	document.addEventListener('DOMContentLoaded', function () {
		var initial = document.querySelector('[data-initial]');
		if (initial) initial.textContent = (document.body.dataset.username || '?').charAt(0).toUpperCase();
		connect();
		if (DASH.can('moderate_names')) api.get('/api/moderation/counts').then(updateBadges).catch(function () {});
		// Staff who manage restarts also see who scheduled it; everyone else gets when and why from the status
		if (DASH.can('server_restart')) api.get('/api/server/restart').then(function (r) { showRestart(r && r.at ? r : null); }).catch(function () {});
		else api.get('/api/status').then(function (s) { showRestart(s.restart); }).catch(function () {});
	});
})();
