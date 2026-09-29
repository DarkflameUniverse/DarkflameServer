/**
 * Shared helpers for every dashboard page.
 *
 * Authentication uses an HttpOnly session cookie, so scripts never see the token. State-changing
 * requests must send X-Requested-With, which the server requires for cookie-authenticated POSTs (CSRF).
 */
(function () {
	'use strict';

	var body = document.body;
	window.DASH = {
		username: body.getAttribute('data-username') || '',
		gmLevel: parseInt(body.getAttribute('data-gm-level') || '0', 10) || 0,
		accountId: parseInt(body.getAttribute('data-account-id') || '0', 10) || 0,
		permissions: (body.getAttribute('data-can') || '').split(' ').filter(Boolean)
	};
	// Whether the user has a permission (the server checks again; this only hides what would be refused)
	DASH.can = function (permission) { return DASH.permissions.indexOf(permission) !== -1; };

	// Escape a value for insertion into HTML. Every player-controlled string must go through this.
	window.esc = function (v) {
		return String(v === null || v === undefined ? '' : v)
			.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;')
			.replace(/"/g, '&quot;').replace(/'/g, '&#39;');
	};

	function handleResponse(r) {
		if (r.status === 401) { window.location.href = '/login'; return Promise.reject(new Error('Not signed in')); }
		var type = r.headers.get('Content-Type') || '';
		if (type.indexOf('application/json') === -1) return r.text().then(function (t) { return { success: r.ok, text: t }; });
		return r.json().then(function (data) {
			if (!r.ok && data.success === undefined) data.success = false;
			return data;
		});
	}

	window.api = {
		get: function (url) {
			return fetch(url, { credentials: 'same-origin', headers: { 'X-Requested-With': 'dashboard' } }).then(handleResponse);
		},
		post: function (url, data) {
			return fetch(url, {
				method: 'POST',
				credentials: 'same-origin',
				headers: { 'Content-Type': 'application/json', 'X-Requested-With': 'dashboard' },
				body: JSON.stringify(data || {})
			}).then(handleResponse);
		},
		// POST and show the outcome as a toast. Resolves with the response when it succeeded.
		// Responses carrying a requestId started a live action on online players; its result is toasted too.
		action: function (url, data, successMessage) {
			return api.post(url, data).then(function (d) {
				if (!d.success) {
					toast(d.error || 'Request failed', 'danger');
					return Promise.reject(new Error(d.error || 'Request failed'));
				}
				if (successMessage) toast(successMessage, 'success');
				// A strike that reached a strike threshold warned, muted or banned the account on its own
				if (d.strikeStep) toast(d.strikeStep, 'warning');
				if (d.strikeStepRequestId && window.Live) {
					Live.waitForAction(d.strikeStepRequestId).then(function (r) { if (r.message) toast(r.message, r.success ? 'info' : 'danger'); });
				}
				if (d.requestId && window.Live) {
					d.result = Live.waitForAction(d.requestId).then(function (r) {
						if (r.message) toast(r.message, r.success ? 'info' : 'danger');
						return r;
					});
				}
				return d;
			});
		}
	};

	/**
	 * Start background work (POST or GET) and wait for its result. Resolves with the result's data; rejects (after
	 * a toast) if starting it or the work itself failed.
	 */
	api.job = function (url, data, method) {
		var start = method === 'GET' ? api.get(url) : api.post(url, data);
		return start.then(function (d) {
			if (!d.success || !d.requestId) {
				toast(d.error || 'Request failed', 'danger');
				return Promise.reject(new Error(d.error || 'Request failed'));
			}
			return Live.waitForResult(d.requestId);
		}).then(function (r) {
			if (!r.success) {
				toast(r.message || 'Failed', 'danger');
				return Promise.reject(new Error(r.message || 'Failed'));
			}
			return r.data === undefined ? r : r.data;
		});
	};

	window.toast = function (message, type) {
		var container = document.getElementById('toast-container');
		if (!container) {
			container = document.createElement('div');
			container.id = 'toast-container';
			container.className = 'toast-container position-fixed bottom-0 end-0 p-3';
			document.body.appendChild(container);
		}
		var el = document.createElement('div');
		el.className = 'toast align-items-center text-bg-' + (type || 'secondary') + ' border-0';
		el.setAttribute('role', 'status');
		var inner = document.createElement('div');
		inner.className = 'd-flex';
		var text = document.createElement('div');
		text.className = 'toast-body';
		text.textContent = message;
		var close = document.createElement('button');
		close.type = 'button';
		close.className = 'btn-close btn-close-white me-2 m-auto';
		close.setAttribute('data-bs-dismiss', 'toast');
		inner.append(text, close);
		el.appendChild(inner);
		container.appendChild(el);
		var t = new bootstrap.Toast(el, { delay: 4000 });
		el.addEventListener('hidden.bs.toast', function () { el.remove(); });
		t.show();
	};

	// Formatting helpers used by table renderers
	window.fmt = {
		unix: function (ts) { return ts ? new Date(ts * 1000).toLocaleString() : '-'; },
		// 1536 -> "1.5 KB"
		bytes: function (n) {
			n = Number(n) || 0;
			var units = ['B', 'KB', 'MB', 'GB', 'TB'], i = 0;
			while (n >= 1024 && i < units.length - 1) { n /= 1024; i++; }
			return (i === 0 ? n : n.toFixed(n < 10 ? 1 : 0)) + ' ' + units[i];
		},
		// Seconds -> "3d 4h", "2h 5m", "7m", "12s"
		duration: function (s) {
			s = Math.max(0, Math.floor(Number(s) || 0));
			var d = Math.floor(s / 86400), h = Math.floor(s % 86400 / 3600), m = Math.floor(s % 3600 / 60);
			if (d) return d + 'd ' + h + 'h';
			if (h) return h + 'h ' + m + 'm';
			return m ? m + 'm' : s + 's';
		},
		date: function (s) {
			if (!s) return '-';
			var d = new Date(String(s).replace(' ', 'T') + (String(s).indexOf('Z') === -1 && String(s).indexOf('T') === -1 ? 'Z' : ''));
			return isNaN(d) ? esc(s) : d.toLocaleString();
		},
		badge: function (text, type) { return '<span class="badge text-bg-' + type + '">' + esc(text) + '</span>'; },
		link: function (href, text) { return '<a href="' + esc(href) + '">' + esc(text) + '</a>'; },
		// A property by name, linked to its page, with a button straight to its 3D view
		property: function (id, name) {
			if (!id || id === '0') return '<span class="text-body-secondary">-</span>';
			return '<span class="text-nowrap">' + fmt.link('/properties/' + id, name || '(unnamed)') +
				' <a href="/properties/' + esc(id) + '/3d" class="btn btn-outline-secondary btn-sm py-0 px-1 ms-1 align-baseline" title="Open the 3D view">3D</a></span>';
		},
		// A character by name, linked; the ID only when the name is unknown (e.g. a deleted character)
		character: function (id, name) {
			if (!id || id === '0') return '<span class="text-body-secondary">-</span>';
			return name ? fmt.link('/characters/' + id, name) : '<span class="text-body-secondary" title="Character not found">Unknown (' + esc(id) + ')</span>';
		},
		// A pet's kind: its icon and CDClient name (rows from the pet name tables carry lot and kind)
		pet: function (lot, kind) {
			if (!lot) return '<span class="text-body-secondary small" title="Not recorded yet: filled in when the owner next loads into a world">Unknown</span>';
			return '<span class="text-nowrap"><img src="/api/icon/' + esc(lot) + '" width="28" height="28" class="me-1 align-middle" alt="" loading="lazy">' +
				esc(kind || ('LOT ' + lot)) + ' <span class="small text-body-secondary">(' + esc(lot) + ')</span></span>';
		},
		zone: function (id, name) {
			return esc(name || ('Zone ' + id)) + ' <span class="small text-body-secondary">(' + esc(id) + ')</span>';
		}
	};

	/**
	 * View choices saved on the account: an input marked data-pref="page.name" (checkbox, radio group, select or text)
	 * starts as it was left and is saved when changed. The saved values come on <body data-prefs>, so they're in place
	 * before the page's own script reads the inputs. Prefs.get/set do the same for choices that aren't a form input.
	 */
	/**
	 * Names for the game's numbered values (gmLevels, inventories, privacy), from the server's enums on
	 * <body data-labels>: Labels.name('privacy', 2) is "Public"; Labels.list('gmLevels') is [{value, name}].
	 */
	window.Labels = (function () {
		var all = {};
		try { all = JSON.parse(document.body.dataset.labels || '{}') || {}; } catch (e) {}
		return {
			list: function (kind) { return all[kind] || []; },
			name: function (kind, value) {
				var match = (all[kind] || []).filter(function (l) { return String(l.value) === String(value); })[0];
				return match ? match.name : null;
			}
		};
	})();

	/**
	 * A search box in place of a long <select>: type to search, pick with the mouse or the arrow keys and Enter.
	 * SearchSelect(input, {search: text => Promise of [{value, label, detail}], value, label, onPick(item)}).
	 * The picked value is on input.dataset.value (empty until something is picked); the box shows its label.
	 * Returns {input, set(value, label)}: set changes the pick from code (nothing: clears it).
	 */
	window.SearchSelect = function (input, options) {
		var wrap = document.createElement('div');
		wrap.className = 'position-relative';
		input.parentNode.insertBefore(wrap, input);
		wrap.appendChild(input);
		var menu = document.createElement('div');
		menu.className = 'dropdown-menu w-100 overflow-auto';
		menu.style.maxHeight = '18rem';
		wrap.appendChild(menu);
		input.setAttribute('autocomplete', 'off');
		input.setAttribute('role', 'combobox');
		input.dataset.value = options.value || '';
		input.value = options.label || '';
		var items = [], active = -1, timer = null, latest = 0, picked = input.value;

		function show(open) { menu.classList.toggle('show', open); input.setAttribute('aria-expanded', open ? 'true' : 'false'); }
		function highlight(i) {
			active = i;
			Array.prototype.forEach.call(menu.children, function (el, n) { el.classList.toggle('active', n === i); if (n === i) el.scrollIntoView({ block: 'nearest' }); });
		}
		function render(list) {
			items = list;
			menu.innerHTML = list.length ? list.map(function (item, n) {
				return '<button type="button" class="dropdown-item text-wrap" data-n="' + n + '">' + esc(item.label) +
					(item.detail ? '<div class="small text-body-secondary">' + esc(item.detail) + '</div>' : '') + '</button>';
			}).join('') : '<div class="dropdown-item-text small text-body-secondary">No matches</div>';
			highlight(list.length ? 0 : -1);
			show(true);
		}
		function lookup() {
			var request = ++latest;
			options.search(input.value.trim()).then(function (list) { if (request === latest && document.activeElement === input) render(list || []); }).catch(function () {});
		}
		function pick(item) {
			input.dataset.value = item.value;
			input.value = picked = item.label;
			show(false);
			if (options.onPick) options.onPick(item);
		}

		input.addEventListener('focus', function () { input.select(); lookup(); });
		input.addEventListener('input', function () { clearTimeout(timer); timer = setTimeout(lookup, 200); });
		input.addEventListener('keydown', function (e) {
			if (e.key === 'ArrowDown' || e.key === 'ArrowUp') {
				e.preventDefault();
				if (!menu.classList.contains('show')) return lookup();
				if (items.length) highlight((active + (e.key === 'ArrowDown' ? 1 : items.length - 1)) % items.length);
			} else if (e.key === 'Enter' && menu.classList.contains('show')) {
				e.preventDefault();
				if (items[active]) pick(items[active]);
			} else if (e.key === 'Escape') {
				show(false);
			}
		});
		// Leaving the box without picking puts back what was picked before
		input.addEventListener('blur', function () { setTimeout(function () { show(false); input.value = picked; }, 150); });
		menu.addEventListener('mousedown', function (e) { e.preventDefault(); });
		menu.addEventListener('click', function (e) {
			var el = e.target.closest('[data-n]');
			if (el) pick(items[Number(el.dataset.n)]);
		});
		return {
			input: input,
			set: function (value, label) { input.dataset.value = value || ''; input.value = picked = label || ''; }
		};
	};

	window.Prefs = (function () {
		var saved = {};
		try { saved = JSON.parse(document.body.dataset.prefs || '{}') || {}; } catch (e) {}
		var pending = {}, timer = null;
		function flush() {
			timer = null;
			var changes = pending;
			pending = {};
			// Public pages (the showcase) are also seen by visitors who aren't signed in: nothing to save for them
			if (!document.body.dataset.username) return;
			api.post('/api/account/preferences', changes).catch(function () {});
		}
		function set(name, value) {
			if (saved[name] === value) return;
			saved[name] = value;
			pending[name] = value;
			if (!timer) timer = setTimeout(flush, 600);
		}
		function valueOf(input) {
			if (input.type === 'checkbox') return input.checked;
			if (input.type === 'radio') {
				var picked = document.querySelector('input[type=radio][name="' + input.name + '"]:checked');
				return picked ? picked.value : null;
			}
			return input.value;
		}
		function apply(input) {
			var name = input.dataset.pref;
			if (!(name in saved)) return;
			var value = saved[name];
			if (input.type === 'checkbox') input.checked = !!value;
			else if (input.type === 'radio') input.checked = input.value === String(value);
			else if (input.tagName !== 'SELECT' || input.querySelector('option[value="' + CSS.escape(String(value)) + '"]')) input.value = value;
		}
		document.querySelectorAll('[data-pref]').forEach(apply);
		document.addEventListener('change', function (e) {
			var input = e.target.closest && e.target.closest('[data-pref]');
			if (input) set(input.dataset.pref, valueOf(input));
		});
		// Don't lose a change made just before leaving the page
		window.addEventListener('pagehide', function () {
			if (!timer) return;
			clearTimeout(timer);
			fetch('/api/account/preferences', { method: 'POST', keepalive: true, credentials: 'same-origin',
				headers: { 'Content-Type': 'application/json', 'X-Requested-With': 'dashboard' }, body: JSON.stringify(pending) });
			pending = {};
			timer = null;
		});
		return {
			get: function (name, fallback) { return name in saved ? saved[name] : fallback; },
			set: set
		};
	})();

	/**
	 * Layout helpers (dashboard.css). --topbar-h is the height of the sticky top bar, for things that stick below it.
	 * .fill-viewport[-lg|-xl] get --fill-top: where they start on the page plus what comes after the page content (its
	 * bottom padding and the footer), so they reach down to the bottom of the window at the top of the page.
	 * Measured again when the window, the page content or the open tab changes.
	 */
	(function () {
		var root = document.documentElement;
		var queued = false;
		function measure() {
			queued = false;
			var bar = document.querySelector('header.sticky-top');
			root.style.setProperty('--topbar-h', (bar ? bar.offsetHeight : 0) + 'px');
			var fills = document.querySelectorAll('.fill-viewport, .fill-viewport-lg, .fill-viewport-xl');
			var main = document.querySelector('main');
			if (!fills.length || !main) return;
			var scroll = window.scrollY;
			var after = parseFloat(getComputedStyle(main).paddingBottom || 0);
			for (var next = main.nextElementSibling; next; next = next.nextElementSibling) after += next.offsetHeight;
			fills.forEach(function (el) {
				if (!el.offsetParent) return; // in a hidden tab: measured when it opens
				var top = el.getBoundingClientRect().top + scroll;
				el.style.setProperty('--fill-top', Math.max(0, Math.round(top + after)) + 'px');
			});
		}
		function queue() {
			if (queued) return;
			queued = true;
			requestAnimationFrame(measure);
		}
		window.fitLayout = measure;
		measure();
		window.addEventListener('resize', queue);
		window.addEventListener('load', queue);
		document.addEventListener('shown.bs.tab', queue);
		document.addEventListener('shown.bs.collapse', queue);
		document.addEventListener('hidden.bs.collapse', queue);
		document.addEventListener('toggle', queue, true); // <details>
		if (window.ResizeObserver) {
			var main = document.querySelector('main');
			if (main) new ResizeObserver(queue).observe(main);
		}
	})();

	/**
	 * A .collapse marked data-pref-open="page.name" (a folding card's body) opens or stays folded as it was last left.
	 */
	document.querySelectorAll('.collapse[data-pref-open]').forEach(function (el) {
		var open = Prefs.get(el.dataset.prefOpen, null);
		if (open === null) return;
		el.classList.toggle('show', !!open);
		document.querySelectorAll('[data-bs-target="#' + CSS.escape(el.id) + '"]').forEach(function (b) {
			b.classList.toggle('collapsed', !open);
			b.setAttribute('aria-expanded', open ? 'true' : 'false');
		});
	});
	document.addEventListener('shown.bs.collapse', function (e) { if (e.target.dataset.prefOpen) Prefs.set(e.target.dataset.prefOpen, true); });
	document.addEventListener('hidden.bs.collapse', function (e) { if (e.target.dataset.prefOpen) Prefs.set(e.target.dataset.prefOpen, false); });

	/**
	 * Breadcrumbs that follow how you actually got to a page, kept per browser tab (sessionStorage).
	 * A detail page has <nav data-crumbs data-crumb-label="..."> holding its natural parents as links (what shows when
	 * the page is opened directly); every other page starts a new trail. Arriving from a page on the trail (by link
	 * or script, seen through the same-origin referrer) continues it from there; returning to a page on the trail
	 * (back, a crumb, reload) cuts it back to that page. Crumbs.label(text) renames the current page once its name
	 * has loaded. A link marked data-crumb-back points at the previous page on the trail.
	 */
	window.Crumbs = (function () {
		var KEY = 'dash.trail', MAX = 7;
		var here = window.location.pathname + window.location.search;
		var nav = document.querySelector('[data-crumbs]');
		function load() {
			try { var t = JSON.parse(sessionStorage.getItem(KEY) || '[]'); return Array.isArray(t) ? t : []; } catch (e) { return []; }
		}
		function save(t) { try { sessionStorage.setItem(KEY, JSON.stringify(t)); } catch (e) { /* storage unavailable */ } }
		function indexOf(t, href) { for (var i = 0; i < t.length; i++) if (t[i] && t[i].href === href) return i; return -1; }
		function titleLabel() {
			var title = document.title.replace(/\s*-\s*DarkflameServer\s*$/, '').trim();
			return !title || title === 'DarkflameServer' ? 'Home' : title;
		}
		var from = '';
		try {
			var ref = document.referrer ? new URL(document.referrer) : null;
			if (ref && ref.origin === window.location.origin) from = ref.pathname + ref.search;
		} catch (e) {}

		var trail = load(), label = nav ? (nav.dataset.crumbLabel || titleLabel()) : titleLabel();
		if (!nav) {
			// Lists and other top-level pages start the trail
			trail = [{ href: here, label: label }];
		} else {
			var at = indexOf(trail, here), came = from && from !== here ? indexOf(trail, from) : -1;
			if (at >= 0) trail = trail.slice(0, at + 1);
			else if (came >= 0) trail = trail.slice(0, came + 1).concat([{ href: here, label: label }]);
			else {
				// Opened directly or from outside the trail: the page's natural parents
				trail = Array.prototype.map.call(nav.querySelectorAll('a[href]'), function (a) {
					return { href: a.getAttribute('href'), label: a.textContent.trim() };
				}).concat([{ href: here, label: label }]);
			}
			if (trail.length > MAX) trail = [trail[0]].concat(trail.slice(trail.length - MAX + 1));
		}
		save(trail);

		function render() {
			if (!nav) return;
			nav.hidden = trail.length < 2;
			var ol = document.createElement('ol');
			ol.className = 'breadcrumb mb-3';
			trail.forEach(function (c, i) {
				var li = document.createElement('li');
				li.className = 'breadcrumb-item' + (i === trail.length - 1 ? ' active' : '');
				if (i === trail.length - 1) { li.setAttribute('aria-current', 'page'); li.textContent = c.label; }
				else { var a = document.createElement('a'); a.href = c.href; a.textContent = c.label; li.appendChild(a); }
				ol.appendChild(li);
			});
			nav.replaceChildren(ol);
			var prev = trail.length > 1 ? trail[trail.length - 2] : null;
			document.querySelectorAll('a[data-crumb-back]').forEach(function (a) {
				if (!prev) return;
				a.href = prev.href;
				a.textContent = '← ' + prev.label;
			});
		}
		render();
		return {
			label: function (text) {
				if (!nav || !text) return;
				trail[trail.length - 1].label = String(text);
				save(trail);
				render();
			},
			previous: function () { return trail.length > 1 ? trail[trail.length - 2] : null; }
		};
	})();

	if (window.jQuery && $.fn.dataTable) {
		// Columns without a custom renderer are inserted as HTML by default; render them as text instead
		$.extend(true, $.fn.dataTable.defaults, {
			columnDefs: [{ targets: '_all', render: $.fn.dataTable.render.text() }]
		});
		/*
		 * Every table remembers how this user last sorted it and how many rows it showed, in this browser only.
		 * Keyed by page (numeric path segments folded, so every account page shares one choice), table id and
		 * username. Search and paging position are deliberately not kept. Tables without an id are skipped.
		 */
		var sortKey = function (settings) {
			if (!settings.sTableId) return null;
			var page = window.location.pathname.replace(/\/\d+(?=\/|$)/g, '/*');
			return 'dash.table:' + (DASH.username || '-') + ':' + page + ':' + settings.sTableId;
		};
		$.extend($.fn.dataTable.defaults, {
			stateSave: true,
			stateDuration: 0,
			stateSaveCallback: function (settings, data) {
				var key = sortKey(settings);
				if (!key) return;
				try { localStorage.setItem(key, JSON.stringify({ order: data.order, length: data.length })); } catch (e) { /* storage unavailable */ }
			},
			stateLoadCallback: function (settings) {
				var key = sortKey(settings), saved = null;
				if (!key) return null;
				try { saved = JSON.parse(localStorage.getItem(key) || 'null'); } catch (e) { return null; }
				if (!saved || typeof saved !== 'object') return null;
				var count = settings.aoColumns.length, state = { time: Date.now() };
				// Drop sorts on columns that no longer exist or can't be sorted (the table changed since)
				if (Array.isArray(saved.order)) state.order = saved.order.filter(function (o) {
					return Array.isArray(o) && o[0] >= 0 && o[0] < count && settings.aoColumns[o[0]].bSortable !== false && (o[1] === 'asc' || o[1] === 'desc');
				});
				if (state.order && !state.order.length) delete state.order;
				if (typeof saved.length === 'number' && (saved.length === -1 || saved.length > 0)) state.length = saved.length;
				return state;
			}
		});
		$.ajaxSetup({ headers: { 'X-Requested-With': 'dashboard' } });
		$(document).on('ajaxError', function (e, xhr) { if (xhr.status === 401) window.location.href = '/login'; });

		/**
		 * Server-side DataTable backed by one of the /api/tables endpoints.
		 * @param extra Optional function returning extra fields to send with each request (filters)
		 * @param liveTable Name of the table in table_changed events, to reload when it changes
		 */
		window.serverTable = function (selector, url, columns, options) {
			options = options || {};
			var stack = $(selector).hasClass('table-stack');
			var labels = stack ? $(selector).find('thead th').map(function () { return $(this).text().trim(); }).get() : [];
			if (stack) columns = columns.map(function (column, i) {
				return $.extend({}, column, { createdCell: function (td) { td.setAttribute('data-label', labels[i] || ''); } });
			});
			// Links like /activity_log#search=123 open with that search filled in
			var initialSearch = (window.location.hash.match(/^#search=(.+)$/) || [])[1];
			var table = $(selector).DataTable($.extend({
				search: { search: initialSearch ? decodeURIComponent(initialSearch) : '' },
				processing: true,
				serverSide: true,
				pageLength: 25,
				lengthMenu: [10, 25, 50, 100],
				order: [[0, 'desc']],
				language: { searchPlaceholder: 'Name or ID' },
				ajax: {
					url: url,
					type: 'POST',
					contentType: 'application/json',
					data: function (d) { return JSON.stringify($.extend(d, options.extra ? options.extra() : {})); }
				},
				columns: columns
			}, options.dataTable || {}));
			if (options.liveTable && window.Live) Live.watchTable(options.liveTable, table);
			return table;
		};
	}
})();
