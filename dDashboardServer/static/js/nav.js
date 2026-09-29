/**
 * Page changes without reloading the whole dashboard.
 *
 * A click on a link to another dashboard page (or a GET form) fetches that page and swaps its <main>, title, page
 * styles and page scripts in, keeping the sidebar, the top bar and the live WebSocket. Back, forward, deep links and
 * #hash filters behave as before. Before a swap, what the old page set up is taken down: its document and window
 * listeners, intervals, Live watchers, DataTables and anything it added to <body>. Page scripts then run again in
 * order; DOMContentLoaded and load handlers they add run once they all have.
 *
 * Falls back to a normal page load when fetching fails on back/forward, the answer isn't a signed-in dashboard page,
 * the permissions or user changed, or either page is one that has to load on its own: module scripts, an import map
 * (the 3D views) or data-nav="reload" anywhere in it. A link with data-nav="off" always loads normally.
 *
 * Nav.go(url) opens a page from a script.
 */
(function () {
	'use strict';

	// ---- Rules (no DOM; tests/dWebTests/nav.test.mjs) ----

	// Paths that are never swapped in: the API, files, the signed-out pages and the metrics
	var NO_SWAP = /^\/(api|css|js|ws|metrics|login|logout|register|forgot_password|reset_password|verify_email|oauth2)(\/|$)|^\/status\/widget(\/|$)|\.[A-Za-z0-9]{1,5}$/;

	var rules = {
		/**
		 * How to follow a link from `here` to `target` (URL objects or {origin, protocol, pathname, search, hash}):
		 * 'swap' to fetch and swap it in, 'hash' when only the fragment of this page changes (left to the browser),
		 * null for a normal load.
		 */
		kind: function (target, here) {
			if (target.origin !== here.origin || !/^https?:$/.test(target.protocol)) return null;
			if (target.pathname === here.pathname && target.search === here.search && target.hash) return 'hash';
			if (NO_SWAP.test(target.pathname)) return null;
			return 'swap';
		},
		// The kind of page a path is, with its numbers folded: every property's 3D view is '/properties/*/3d'
		pageKind: function (path) {
			return String(path).replace(/\/\d+(?=\/|$)/g, '/*');
		},
		// Whether a <script type> is JavaScript that runs as a classic script (JSON data and modules aren't)
		runnable: function (type) {
			return !type || /^(text|application)\/(x-)?(java|ecma)script$/i.test(type.trim());
		},
		// Whether two lists of child keys ('DIV#id', '#text') line up one to one
		sameKeys: function (a, b) {
			if (a.length !== b.length) return false;
			for (var i = 0; i < a.length; i++) if (a[i] !== b[i]) return false;
			return true;
		}
	};
	window.NavRules = rules;

	if (typeof document === 'undefined' || !window.fetch || !window.DOMParser || !window.history || !history.pushState) return;
	var main = document.querySelector('main'), sidebar = document.getElementById('sidebar');
	if (!main || !sidebar) return;

	// ---- What the current page sets up, so it can be taken down ----

	var nativeAdd = EventTarget.prototype.addEventListener, nativeRemove = EventTarget.prototype.removeEventListener;
	var nativeSetInterval = window.setInterval, nativeClearInterval = window.clearInterval;
	var nativeReplace = history.replaceState, nativePush = history.pushState;
	var nativeFetch = window.fetch;

	var page = newPage();
	var runningScripts = false, readyQueue = [];
	var keepInBody = Array.prototype.slice.call(document.body.children);
	var inFlight = 0;  // fetches of any kind, to know when a swapped-in page has loaded its data

	function newPage() { return { listeners: [], intervals: [], hard: null }; }

	function isJqueryHandle(target, fn) {
		try { var data = window.jQuery && jQuery._data && jQuery._data(target); return !!(data && data.handle === fn); } catch (e) { return false; }
	}
	[document, window].forEach(function (target) {
		target.addEventListener = function (type, fn, options) {
			// Scripts run after a swap still wait for the document to be ready: run those handlers once they're all in
			if (runningScripts && (type === 'DOMContentLoaded' || (type === 'load' && target === window))) {
				readyQueue.push({ target: target, type: type, fn: fn });
				return;
			}
			nativeAdd.call(target, type, fn, options);
			// jQuery keeps one handler per element and event for all of its own; DataTables take theirs off themselves
			if (fn && !isJqueryHandle(target, fn)) page.listeners.push([target, type, fn, options]);
		};
	});
	window.setInterval = function () {
		var id = nativeSetInterval.apply(window, arguments);
		page.intervals.push(id);
		return id;
	};
	window.fetch = function () {
		inFlight++;
		return nativeFetch.apply(window, arguments).finally(function () { inFlight--; });
	};

	// ---- History ----

	var rendered = location.pathname + location.search;  // the page shown now
	var scrolls = {};  // history entry key -> scroll position
	var entryKey = 0;
	function newKey() { return Date.now().toString(36) + '.' + (++entryKey); }
	function state(extra) {
		var s = {};
		var current = history.state;
		if (current && typeof current === 'object') for (var k in current) s[k] = current[k];
		if (!s.dashNav) s.dashNav = newKey();
		if (extra) for (var e in extra) s[e] = extra[e];
		return s;
	}
	// Pages set the fragment with replaceState(null, ...): keep this entry's key so back and forward still find it
	history.replaceState = function (s, title, url) {
		nativeReplace.call(history, s === null || s === undefined ? state() : s, title, url);
		rendered = location.pathname + location.search;
	};
	history.pushState = function (s, title, url) {
		nativePush.call(history, s, title, url);
		rendered = location.pathname + location.search;
	};
	nativeReplace.call(history, state(), '');
	// Scroll positions are put back by this file, after the page's content is in (the browser would do it too early).
	// For a reload they're kept in sessionStorage.
	if ('scrollRestoration' in history) history.scrollRestoration = 'manual';
	var SCROLL_KEY = 'dash.scroll';
	try {
		var savedScrolls = JSON.parse(sessionStorage.getItem(SCROLL_KEY) || '{}') || {};
		for (var k in savedScrolls) scrolls[k] = savedScrolls[k];
	} catch (e) { /* storage unavailable */ }
	var startKey = history.state.dashNav;
	if (typeof scrolls[startKey] === 'number') {
		nativeAdd.call(window, 'load', function () {
			var y = scrolls[startKey];
			if (window.scrollY !== 0) return;  // something else scrolled already (Live's own restore, a #fragment)
			window.scrollTo(0, y);
			var landed = window.scrollY;
			whenLoaded(function () { if (window.scrollY === landed) window.scrollTo(0, y); });
		});
	}
	nativeAdd.call(window, 'pagehide', function () {
		if (history.state && history.state.dashNav) scrolls[history.state.dashNav] = window.scrollY;
		var keys = Object.keys(scrolls).slice(-50), keep = {};
		keys.forEach(function (key) { keep[key] = scrolls[key]; });
		try { sessionStorage.setItem(SCROLL_KEY, JSON.stringify(keep)); } catch (e) { /* storage unavailable */ }
	});
	var scrollTimer = null;
	nativeAdd.call(window, 'scroll', function () {
		if (scrollTimer) return;
		scrollTimer = setTimeout(function () {
			scrollTimer = null;
			if (history.state && history.state.dashNav) scrolls[history.state.dashNav] = window.scrollY;
		}, 100);
	}, { passive: true });

	// ---- Loading state and errors ----

	var progress = document.createElement('div');
	progress.className = 'nav-progress';
	progress.setAttribute('aria-hidden', 'true');
	progress.dataset.navKeep = '';
	document.body.appendChild(progress);
	keepInBody.push(progress);
	var loadingTimer = null;
	function setLoading(on) {
		clearTimeout(loadingTimer);
		if (on) loadingTimer = setTimeout(function () { document.documentElement.classList.add('nav-loading'); main.setAttribute('aria-busy', 'true'); }, 120);
		else { document.documentElement.classList.remove('nav-loading'); main.removeAttribute('aria-busy'); }
	}

	function showError(url, message) {
		var old = document.getElementById('nav-error');
		if (old) old.remove();
		var box = document.createElement('div');
		box.id = 'nav-error';
		box.className = 'alert alert-danger d-flex flex-wrap align-items-center gap-2';
		box.setAttribute('role', 'alert');
		var text = document.createElement('span');
		text.className = 'me-auto';
		text.textContent = message;
		var retry = document.createElement('button');
		retry.type = 'button';
		retry.className = 'btn btn-sm btn-light';
		retry.textContent = 'Try again';
		retry.addEventListener('click', function () { box.remove(); go(url); });
		var plain = document.createElement('a');
		plain.className = 'btn btn-sm btn-outline-light';
		plain.href = url;
		plain.dataset.nav = 'off';
		plain.textContent = 'Open it normally';
		box.append(text, retry, plain);
		main.insertBefore(box, main.firstChild);
		window.scrollTo(0, 0);
	}

	// ---- Reading a fetched page ----

	function isHard(doc) {
		return !!doc.querySelector('script[type="module"], script[type="importmap"], [data-nav="reload"]');
	}
	function currentIsHard() {
		if (page.hard === null) page.hard = isHard(document);
		return page.hard;
	}
	// Why a fetched page can't be swapped in (a normal load then), or '' when it can
	function unusable(doc) {
		if (!doc.querySelector('main') || !doc.getElementById('sidebar') || !doc.getElementById('page-scripts')) return 'not a dashboard page';
		var a = document.body, b = doc.body;
		if (a.dataset.username !== b.dataset.username || a.dataset.can !== b.dataset.can || a.dataset.gmLevel !== b.dataset.gmLevel) return 'the account changed';
		if (isHard(doc)) return 'loads on its own';
		return '';
	}
	function fetchPage(url, signal) {
		return fetch(url, { credentials: 'same-origin', headers: { Accept: 'text/html' }, signal: signal }).then(function (r) {
			if (r.status === 401) { window.location.href = '/login'; throw new Error('Not signed in'); }
			var type = r.headers.get('Content-Type') || '';
			if (type.indexOf('text/html') === -1) return { url: r.url || url, doc: null };
			return r.text().then(function (html) {
				return { url: r.url || url, doc: new DOMParser().parseFromString(html, 'text/html') };
			});
		});
	}

	// ---- Taking the old page down ----

	function leave() {
		try { document.dispatchEvent(new CustomEvent('dash:leave')); } catch (e) { console.error(e); }
		if (window.Live && Live.resetPage) Live.resetPage();
		page.listeners.forEach(function (l) { nativeRemove.call(l[0], l[1], l[2], l[3]); });
		page.intervals.forEach(function (id) { nativeClearInterval.call(window, id); });
		if (window.jQuery && jQuery.fn.dataTable) {
			try { jQuery.fn.dataTable.tables({ api: true }).destroy(); } catch (e) { console.error(e); }
		}
		// Dialogs: the page's go with it; shared ones (Decide, the AI helper) are closed
		document.querySelectorAll('.modal.show').forEach(function (el) {
			var modal = window.bootstrap && bootstrap.Modal.getInstance(el);
			if (el.hasAttribute('data-nav-keep')) { if (modal) modal.hide(); return; }
			if (modal) modal.dispose();
		});
		document.querySelectorAll('.modal-backdrop, .tooltip, .popover').forEach(function (el) { el.remove(); });
		if (!document.querySelector('.modal.show[data-nav-keep]')) {
			document.body.classList.remove('modal-open');
			document.body.style.removeProperty('overflow');
			document.body.style.removeProperty('padding-right');
		}
		Array.prototype.slice.call(document.body.children).forEach(function (el) {
			if (keepInBody.indexOf(el) !== -1 || el.id === 'page-scripts' || el.id === 'toast-container' || el.hasAttribute('data-nav-keep') || el.tagName === 'SCRIPT') return;
			el.remove();
		});
		page = newPage();
	}

	// ---- Page styles (between the page-head markers in base.jinja2) ----

	function pageHead(doc) {
		var start = doc.querySelector('meta[name="page-head-start"]'), end = doc.querySelector('meta[name="page-head-end"]');
		var nodes = [];
		for (var n = start && start.nextSibling; n && n !== end; n = n.nextSibling) if (n.nodeType === 1) nodes.push(n);
		return nodes;
	}
	// Put the new page's styles in (ready once its style sheets have loaded) and say which old ones to take out after
	function prepareHead(doc) {
		var old = pageHead(document), end = document.querySelector('meta[name="page-head-end"]');
		var keep = [], loads = [];
		pageHead(doc).forEach(function (n) {
			var same = old.filter(function (o) { return keep.indexOf(o) === -1 && o.outerHTML === n.outerHTML; })[0];
			if (same) { keep.push(same); return; }
			var el = document.importNode(n, true);
			if (el.tagName === 'LINK' && /stylesheet/i.test(el.rel)) {
				loads.push(new Promise(function (resolve) {
					el.addEventListener('load', resolve);
					el.addEventListener('error', resolve);
					setTimeout(resolve, 1500);
				}));
			}
			end.parentNode.insertBefore(el, end);
		});
		return { ready: Promise.all(loads), stale: old.filter(function (o) { return keep.indexOf(o) === -1; }) };
	}

	// ---- The sidebar ----

	function links(nav) {
		return Array.prototype.map.call(nav.querySelectorAll('a[href]'), function (a) { return a.getAttribute('href'); });
	}
	function updateSidebar(doc) {
		var next = doc.getElementById('sidebar');
		if (!rules.sameKeys(links(sidebar), links(next))) {
			// Other pages are allowed now: the server's menu, with its groups as the user left them
			sidebar.replaceChildren.apply(sidebar, Array.prototype.map.call(next.childNodes, function (n) { return document.importNode(n, true); }));
			if (window.Live && Live.refreshBadges) Live.refreshBadges();
		} else {
			var mine = sidebar.querySelectorAll('a[href]'), theirs = next.querySelectorAll('a[href]');
			for (var i = 0; i < mine.length; i++) {
				var active = theirs[i].classList.contains('active');
				mine[i].classList.toggle('active', active);
				if (active) mine[i].setAttribute('aria-current', 'page'); else mine[i].removeAttribute('aria-current');
			}
		}
		if (window.Sidebar) Sidebar.apply();
		var offcanvas = window.bootstrap && bootstrap.Offcanvas.getInstance(sidebar);
		if (offcanvas) offcanvas.hide();
	}

	// ---- Swapping a page in ----

	function scriptsIn(root) {
		return Array.prototype.filter.call(root.querySelectorAll('script'), function (s) { return rules.runnable(s.getAttribute('type')); });
	}
	function runScripts(list) {
		runningScripts = true;
		return list.reduce(function (done, old) {
			return done.then(function () {
				if (!old.parentNode) return;
				var s = document.createElement('script');
				Array.prototype.forEach.call(old.attributes, function (a) { s.setAttribute(a.name, a.value); });
				if (!old.src) {
					s.text = old.text;
					old.parentNode.replaceChild(s, old);
					return;
				}
				s.async = false;
				return new Promise(function (resolve) {
					s.addEventListener('load', resolve);
					s.addEventListener('error', resolve);
					old.parentNode.replaceChild(s, old);
				});
			});
		}, Promise.resolve()).then(function () {
			runningScripts = false;
			var queue = readyQueue;
			readyQueue = [];
			queue.forEach(function (q) {
				try { q.fn.call(q.target, new Event(q.type)); } catch (e) { console.error(e); }
			});
		}, function (e) { runningScripts = false; readyQueue = []; throw e; });
	}

	// Once the page's first requests are done (or after a few seconds): scroll again, the content has its height now
	function whenLoaded(fn) {
		var until = Date.now() + 3000;
		(function check() {
			var busy = inFlight > 0 || (window.jQuery && jQuery.active > 0);
			if (busy && Date.now() < until) setTimeout(check, 100); else fn();
		})();
	}

	/**
	 * Swap the fetched page in. options: push (a new history entry) or replace, from (the page it was reached from),
	 * scroll (restore to this position).
	 */
	function swap(doc, url, options) {
		var head = prepareHead(doc);
		return head.ready.then(function () {
			leave();
			head.stale.forEach(function (n) { n.remove(); });
			if (options.push) {
				if (history.state && history.state.dashNav) scrolls[history.state.dashNav] = window.scrollY;
				nativePush.call(history, { dashNav: newKey() }, '', url);
			} else if (options.replace) nativeReplace.call(history, state(), '', url);
			rendered = location.pathname + location.search;
			document.title = doc.title;
			updateSidebar(doc);

			var banner = document.getElementById('restart-banner');
			if (banner) banner.remove();
			var nextMain = doc.querySelector('main');
			var nodes = Array.prototype.slice.call(nextMain.childNodes).map(function (n) { return document.adoptNode(n); });
			main.replaceChildren.apply(main, nodes);
			if (banner) main.insertBefore(banner, main.firstChild);

			var scripts = document.getElementById('page-scripts');
			scripts.replaceChildren.apply(scripts, Array.prototype.slice.call(doc.getElementById('page-scripts').childNodes).map(function (n) { return document.adoptNode(n); }));

			if (window.Prefs && Prefs.apply) Prefs.apply(main);
			if (window.Crumbs && Crumbs.start) Crumbs.start(options.from || '');
			var hash = new URL(url, location.href).hash;
			var target = hash ? document.getElementById(decodeURIComponent(hash.slice(1))) : null;
			var y = options.scroll, landed = 0;
			if (typeof y === 'number') {
				// The page gets its full height once its data is in: hold enough height until then to scroll back to y
				main.style.minHeight = Math.max(0, y + window.innerHeight - (main.getBoundingClientRect().top + window.scrollY)) + 'px';
				window.scrollTo(0, y);
				landed = window.scrollY;
			} else if (target) target.scrollIntoView();
			else window.scrollTo(0, 0);
			main.focus({ preventScroll: true });

			return runScripts(scriptsIn(main).concat(scriptsIn(scripts))).then(function () {
				if (window.fitLayout) fitLayout();
				try { document.dispatchEvent(new CustomEvent('dash:page')); } catch (e) { console.error(e); }
				if (typeof y === 'number') {
					whenLoaded(function () {
						if (window.scrollY === landed) window.scrollTo(0, y);  // unless the user scrolled meanwhile
						main.style.minHeight = '';
					});
				}
			});
		});
	}

	// ---- Leaving a page with unsaved changes ----

	function confirmLeave() {
		var asked = false;
		page.listeners.forEach(function (l) {
			if (asked || l[0] !== window || l[1] !== 'beforeunload') return;
			var e = { type: 'beforeunload', defaultPrevented: false, returnValue: '', preventDefault: function () { this.defaultPrevented = true; } };
			try {
				var r = (typeof l[2] === 'function' ? l[2] : l[2].handleEvent).call(window, e);
				if (e.defaultPrevented || e.returnValue || (typeof r === 'string' && r)) asked = true;
			} catch (err) { /* ignore */ }
		});
		if (typeof window.onbeforeunload === 'function') {
			var ev = { returnValue: '', preventDefault: function () { ev.returnValue = 'x'; } };
			try { if (window.onbeforeunload(ev) || ev.returnValue) asked = true; } catch (err) { /* ignore */ }
		}
		return !asked || window.confirm('Leave this page? Changes you made may not be saved.');
	}

	// ---- Going somewhere ----

	var sequence = 0, controller = null;
	var loadsOnItsOwn = {};  // kinds of page found to need a normal load, so they aren't fetched twice

	/**
	 * Open url. options: pop (back/forward: the address already changed), replace (no new history entry),
	 * scroll (where to scroll to afterwards).
	 */
	function go(url, options) {
		options = options || {};
		url = new URL(url, location.href).href;
		var kind = rules.kind(new URL(url), location);
		if (kind === 'hash' && !options.pop) { location.assign(url); return Promise.resolve(); }
		if (currentIsHard() || kind === null || loadsOnItsOwn[rules.pageKind(new URL(url).pathname)]) {
			if (options.pop || options.replace) location.replace(url); else location.assign(url);
			return Promise.resolve();
		}
		if (!confirmLeave()) {
			// Back/forward already changed the address: put this page's back
			if (options.pop) nativePush.call(history, { dashNav: newKey() }, '', rendered);
			return Promise.resolve();
		}
		var id = ++sequence;
		if (controller) controller.abort();
		controller = window.AbortController ? new AbortController() : null;
		var from = rendered;
		setLoading(true);
		var old = document.getElementById('nav-error');
		if (old) old.remove();
		return fetchPage(url, controller && controller.signal).then(function (r) {
			if (id !== sequence) return;
			// fetch drops the fragment; keep the one asked for when there was no redirect elsewhere
			var target = new URL(r.url);
			var asked = new URL(url);
			if (!target.hash && target.pathname === asked.pathname && target.search === asked.search) target.hash = asked.hash;
			var why = r.doc ? unusable(r.doc) : 'not a page';
			if (why) {
				if (why === 'loads on its own') loadsOnItsOwn[rules.pageKind(target.pathname)] = true;
				if (options.pop) location.replace(target.href); else location.assign(target.href);
				return;
			}
			return swap(r.doc, target.href, { push: !options.pop && !options.replace, replace: !!options.replace || !!options.pop, from: from, scroll: options.scroll });
		}).catch(function (e) {
			if (id !== sequence || (e && e.name === 'AbortError')) return;
			if (options.pop) { location.replace(url); return; }
			console.error(e);
			showError(url, 'Couldn\'t open that page' + (navigator.onLine === false ? ': you\'re offline.' : '. The server may be restarting.'));
		}).finally(function () {
			if (id === sequence) setLoading(false);
		});
	}

	window.Nav = { go: go };

	// ---- Links, forms, back and forward ----

	nativeAdd.call(window, 'click', function (e) {
		if (e.defaultPrevented || e.button !== 0 || e.metaKey || e.ctrlKey || e.shiftKey || e.altKey) return;
		var a = e.target.closest && e.target.closest('a[href]');
		if (!a || (a.target && a.target !== '_self') || a.hasAttribute('download') || a.getAttribute('data-nav') === 'off' || a.hasAttribute('data-bs-toggle')) return;
		var url;
		try { url = new URL(a.href, location.href); } catch (err) { return; }
		if (rules.kind(url, location) !== 'swap') return;
		e.preventDefault();
		// The page that is open already: shown again, without a second history entry
		go(url.href, { replace: url.href === location.href });
	});
	nativeAdd.call(window, 'submit', function (e) {
		var form = e.target;
		if (e.defaultPrevented || !form || form.tagName !== 'FORM' || (form.getAttribute('method') || 'get').toLowerCase() !== 'get') return;
		if ((form.target && form.target !== '_self') || form.getAttribute('data-nav') === 'off') return;
		var url = new URL(form.getAttribute('action') || location.pathname, location.href);
		var data;
		try { data = new FormData(form, e.submitter); } catch (err) { data = new FormData(form); }
		url.search = new URLSearchParams(data).toString();
		url.hash = '';
		if (rules.kind(url, location) !== 'swap') return;
		e.preventDefault();
		go(url.href);
	});
	nativeAdd.call(window, 'popstate', function (e) {
		if (currentIsHard()) return;  // its own history entries (the UGC page)
		if (location.pathname + location.search === rendered) return;  // only the fragment changed
		var key = e.state && e.state.dashNav;
		go(location.href, { pop: true, scroll: key && key in scrolls ? scrolls[key] : undefined });
	});
})();
