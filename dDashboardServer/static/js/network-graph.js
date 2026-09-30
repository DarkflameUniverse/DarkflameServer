/**
 * The Network page's diagram model, without the DOM so it can be tested with node (tests/dWebTests/network-graph.test.mjs).
 *
 * build(summary, options) turns what /api/diagnostics/network (and the `traffic` WebSocket topic) sends into nodes and
 * links: game clients on the left, auth, chat, the worlds (one node per zone) and any other reporting server in the
 * middle, then master, the dashboard and the UGC server, and web clients on the right. Nodes come from whatever servers
 * report, so a new kind of server shows up by itself. Each link has its rates each way: `fwd` from `from` to `to`,
 * `back` the other way, in packets (HTTP links: requests) and bytes per second; `exact` says whether they are measured
 * on that link or worked out from a server's totals (older servers report no split).
 *
 * Groups (Game clients, Web clients, a zone with several instances) open in place: the box gets `open`, its `members`
 * (single connections, or instances) and `shown` (the ids of those matching the group's filter). Each shown member is
 * a link end of its own, and the open box itself has none; `graph.members` finds a member by id.
 *
 * layout(graph, width, moved, scroll) places the nodes in six columns in a fixed order, grows open boxes by their rows
 * and gives each link its path; rows in view of an open box's list are anchors (`anchors`).
 */
(function (root) {
	'use strict';

	var COLUMNS = [0, 1, 2, 3, 4, 5]; // game clients | auth, worlds, others | chat | master | dashboard, UGC | web clients
	// Chat has a column of its own between the worlds and master: every world has a link to it
	// An open group's box: its header, the filter, the rows (at most MAX_ROWS in view, the rest scroll) and a margin
	var GROUP = { head: 58, filter: 30, row: 30, maxRows: 8, foot: 6 }; // head: a name on up to two lines and one line under it

	function num(v) { return typeof v === 'number' && isFinite(v) ? v : 0; }
	function rates(r) { r = r || {}; return { packets_in: num(r.packets_in), packets_out: num(r.packets_out), bytes_in: num(r.bytes_in), bytes_out: num(r.bytes_out) }; }
	function any(r) { return r.packets_in > 0 || r.packets_out > 0 || r.bytes_in > 0 || r.bytes_out > 0; }

	// The node a server belongs to (a zone's instances are all in the zone's box)
	function nodeOf(server) {
		switch (server.type) {
			case 'AUTH': return { id: 'auth', kind: 'auth', label: 'Auth', column: 1, order: 0 };
			case 'CHAT': return { id: 'chat', kind: 'chat', label: 'Chat', column: 2, order: 0 };
			case 'MASTER': return { id: 'master', kind: 'master', label: 'Master', column: 3, order: 0 };
			case 'DASHBOARD': return { id: 'dashboard', kind: 'dashboard', label: 'Dashboard', column: 4, order: 0 };
			case 'UGC': return { id: 'ugc', kind: 'ugc', label: 'UGC server', column: 4, order: 1 };
			case 'WORLD': {
				var zone = num(server.zone);
				// "World 1200 Nimbus Station #3" -> "World 1200 Nimbus Station"
				var zoneLabel = String(server.label || ('World ' + zone)).replace(/\s+#\d+$/, '');
				return { id: 'zone:' + zone, kind: 'zone', label: zoneLabel, column: 1, order: 2 + zone, zone: zone };
			}
			default: return { id: server.key, kind: 'other', label: server.label || server.key, column: 1, order: 1e9 };
		}
	}

	// Which group a connection from /api/diagnostics/network/connections is a member of: players are game clients, and
	// so are HTTP clients of the UGC server only (game clients fetching models; the closed box counts them there too)
	function peerGroup(peer, servers) {
		if (peer.kind === 'game') return 'clients';
		if (peer.kind !== 'web') return null;
		var list = peer.servers || [];
		var ugcOnly = list.length > 0 && list.every(function (e) { return servers[e.server] && servers[e.server].type === 'UGC'; });
		return ugcOnly ? 'clients' : 'web';
	}

	// What a group's filter matches a member by
	function matches(member, filter) {
		filter = String(filter || '').trim().toLowerCase();
		if (!filter) return true;
		return filter.split(/\s+/).every(function (word) { return member.search.indexOf(word) !== -1; });
	}

	function build(summary, options) {
		options = options || {};
		var servers = (summary && summary.servers) || {};
		var keys = Object.keys(servers).sort();
		var expanded = options.expanded || {}, filters = options.filters || {};
		var nodes = {}, edges = {}, members = {};

		function node(spec) {
			var n = nodes[spec.id];
			if (!n) {
				n = nodes[spec.id] = { id: spec.id, kind: spec.kind, label: spec.label, column: spec.column, order: spec.order, zone: spec.zone, servers: [], instances: 0,
					connections: 0, pingSum: 0, pingCount: 0, resends: 0, resendQueue: 0, workersBusy: 0, workersThreads: 0, workersQueued: 0, websockets: 0,
					packetsIn: 0, packetsOut: 0, bytesIn: 0, bytesOut: 0, http: 0, noSplit: false, open: false, members: [], shown: [] };
			}
			return n;
		}
		function member(group, spec) {
			spec.group = group.id;
			spec.connections = spec.connections || 0;
			spec.pingSum = spec.pingSum || 0;
			spec.pingCount = spec.pingCount || 0;
			spec.resends = spec.resends || 0;
			members[spec.id] = spec;
			group.members.push(spec);
			group.open = true;
			return spec;
		}
		// Adds a link's rates; links between the same two nodes add up (every instance of a zone)
		function edge(from, to, fwdPackets, fwdBytes, backPackets, backBytes, exact, kind, note) {
			var id = from + '>' + to + (kind && kind !== 'packets' ? ':' + kind : '');
			var e = edges[id];
			if (!e) e = edges[id] = { id: id, from: from, to: to, kind: kind || 'packets', exact: true, notes: [], fwd: { packets: 0, bytes: 0 }, back: { packets: 0, bytes: 0 }, bytesKnown: { fwd: true, back: true } };
			e.fwd.packets += num(fwdPackets);
			e.back.packets += num(backPackets);
			if (fwdBytes == null) e.bytesKnown.fwd = false; else e.fwd.bytes += num(fwdBytes);
			if (backBytes == null) e.bytesKnown.back = false; else e.back.bytes += num(backBytes);
			if (!exact) e.exact = false;
			if (note && e.notes.indexOf(note) === -1) e.notes.push(note);
			return e;
		}
		// Where a link to a server ends: the server's box, or its row while its zone is open
		function endOf(server) { return server.type === 'WORLD' && expanded[num(server.zone)] ? server.key : nodeOf(server).id; }

		var clients = node({ id: 'clients', kind: 'clients', label: 'Game clients', column: 0, order: 0 });
		var web = node({ id: 'web', kind: 'web', label: 'Web clients', column: 5, order: 0 });
		var hasChat = keys.some(function (k) { return servers[k].type === 'CHAT'; });

		keys.forEach(function (key) {
			var s = servers[key];
			var n = node(nodeOf(s));
			var link = s.link || {}, gauges = s.gauges || {};
			// Links end at the zone's box, or at the instance's row while the zone is open
			var id = s.type === 'WORLD' ? endOf(s) : n.id;
			n.servers.push(key);
			if (s.type === 'WORLD') {
				n.instances++;
				if (id !== n.id) {
					member(n, { id: key, kind: 'world', label: '#' + num(s.instance), title: s.label || key, server: key, instance: num(s.instance),
						connections: num(link.connections), pingSum: num(link.connections) > 0 ? num(link.ping_ms) : 0, pingCount: num(link.connections) > 0 ? 1 : 0,
						resends: num(link.resends), search: (String(s.label || key) + ' #' + num(s.instance) + ' ' + key).toLowerCase() });
				}
			}
			n.connections += num(link.connections);
			if (num(link.connections) > 0) { n.pingSum += num(link.ping_ms); n.pingCount++; }
			n.resends += num(link.resends);
			n.resendQueue += num(link.resend_queue);
			n.workersBusy += num(gauges.workers_busy);
			n.workersThreads += num(gauges.workers_threads);
			n.workersQueued += num(gauges.workers_queued);
			n.websockets += num(gauges.websocket_clients);
			n.packetsIn += num(s.packets_in); n.packetsOut += num(s.packets_out);
			n.bytesIn += num(s.bytes_in); n.bytesOut += num(s.bytes_out);
			n.http += num(s.http);

			var split = s.split;
			if (!split) n.noSplit = true;
			var totals = rates(s);
			var ESTIMATED = 'from the server\'s totals (it reports no split by peer)';

			// Its master link, measured on its side (master itself is the other end of all of them)
			if (s.type !== 'MASTER') {
				if (split) {
					var m = rates(split.master);
					if (any(m)) edge(id, 'master', m.packets_out, m.bytes_out, m.packets_in, m.bytes_in, true);
				} else if (s.type === 'DASHBOARD' || s.type === 'UGC' || s.type === 'CHAT') {
					// Nearly all of their packets are with master (chat's are with the worlds too)
					edge(id, 'master', totals.packets_out, totals.bytes_out, totals.packets_in, totals.bytes_in, false, 'packets', ESTIMATED);
				}
			}

			if (s.type === 'DASHBOARD' || s.type === 'UGC') {
				// HTTP: requests from browsers and game clients (all but other servers'), and the bodies answered
				var fromServers = split ? num(split.http_from_servers) : 0, fromServersBytes = split ? num(split.http_from_servers_bytes_out) : 0;
				var requests = Math.max(0, num(s.http) - fromServers), bytesOut = Math.max(0, num(s.http_bytes_out) - fromServersBytes);
				var who = s.type === 'UGC' ? 'clients' : 'web';
				if (requests > 0 || bytesOut > 0) edge(who, id, requests, null, requests, bytesOut, !!split, 'http', split ? '' : 'other servers\' requests not told apart (older server)');
				// The dashboard's own requests to the UGC server, counted where they were made
				if (split && num(split.http_out) > 0) edge(id, 'ugc', split.http_out, null, split.http_out, split.http_out_bytes_in, true, 'http');
				return;
			}
			if (s.type === 'MASTER') return;

			if (split) {
				var c = rates(split.clients);
				// A player standing still sends nothing for a while: their connection is still drawn (idle)
				var players = (s.type === 'AUTH' || s.type === 'WORLD') ? Math.max(0, num((s.link || {}).connections) - 1) : 0;
				if (any(c) || players > 0) edge('clients', id, c.packets_in, c.bytes_in, c.packets_out, c.bytes_out, true);
				var o = rates(split.servers);
				// A world's "other servers" is its chat link; chat's are the worlds, drawn from their side
				if (s.type === 'WORLD' && any(o)) edge(id, hasChat ? 'chat' : 'master', o.packets_out, o.bytes_out, o.packets_in, o.bytes_in, hasChat, 'packets', hasChat ? '' : 'chat link, chat not reporting');
				else if (s.type !== 'CHAT' && any(o)) edge(id, 'master', o.packets_out, o.bytes_out, o.packets_in, o.bytes_in, false, 'servers', 'with other servers, drawn to master');
			} else if (s.type !== 'CHAT') {
				edge('clients', id, totals.packets_in, totals.bytes_in, totals.packets_out, totals.bytes_out, false, 'packets', ESTIMATED);
			}
		});

		// Game clients and web clients: connections from the servers they use
		keys.forEach(function (key) {
			var s = servers[key], link = s.link || {};
			if (s.type === 'AUTH' || s.type === 'WORLD') clients.connections += Math.max(0, num(link.connections) - 1); // less its master link
		});
		web.websockets = keys.reduce(function (t, k) { return t + (servers[k].type === 'DASHBOARD' ? num((servers[k].gauges || {}).websocket_clients) : 0); }, 0);

		// An open Game clients or Web clients box: each connection (from /api/diagnostics/network/connections) is a row
		// with its own links, in place of the box's. Until the list has loaded the box keeps its summed links.
		var peersLoaded = Array.isArray(options.peers);
		var peerOpen = { clients: !!options.clientsExpanded && peersLoaded, web: !!options.webExpanded && peersLoaded };
		if (peerOpen.clients) clients.open = true;
		if (peerOpen.web) web.open = true;
		var seen = {};
		(peersLoaded ? options.peers : []).forEach(function (peer) {
			var groupId = peerGroup(peer, servers);
			if (!groupId || !peerOpen[groupId]) return;
			var base = 'peer:' + groupId + ':' + (peer.account_id ? peer.account_id + '@' : '') + peer.address, id = base;
			for (var k = 2; seen[id]; k++) id = base + '#' + k;
			seen[id] = true;
			var names = [peer.character, peer.account, peer.user];
			var p = member(nodes[groupId], { id: id, kind: 'peer', label: String(peer.character || peer.account || peer.user || peer.address || ''), peer: peer,
				search: names.concat([options.addressesShown ? peer.address : ''], (peer.servers || []).map(function (e) { return e.label; }))
					.filter(Boolean).join(' ').toLowerCase() });
			(peer.servers || []).forEach(function (entry) {
				var target = servers[entry.server];
				if (!target) return;
				var to = endOf(target);
				if (entry.http) edge(id, to, entry.packets_in, entry.bytes_in, entry.packets_out, entry.bytes_out, true, 'peer-http');
				else edge(id, to, entry.packets_in, entry.bytes_in, entry.packets_out, entry.bytes_out, true, 'peer', 'RakNet datagrams of this connection, acknowledgements and resends included');
				if (!entry.http) { p.connections++; p.pingSum += num(entry.ping_ms); p.pingCount++; p.resends += num(entry.resends); }
			});
		});

		var list = Object.keys(nodes).map(function (k) { return nodes[k]; });
		// Open groups: members in a steady order (a list that is read and scrolled should not reshuffle every update),
		// and the ones matching the group's filter
		list.forEach(function (n) {
			if (!n.open) return;
			n.members.sort(function (a, b) {
				return a.kind === 'world' ? a.instance - b.instance : String(a.label).localeCompare(String(b.label)) || (a.id < b.id ? -1 : a.id > b.id ? 1 : 0);
			});
			n.shown = n.members.filter(function (m) { return matches(m, filters[n.id]); }).map(function (m) { return m.id; });
		});
		list.forEach(function (n) { n.ping = n.pingCount ? Math.round(n.pingSum / n.pingCount) : null; });
		Object.keys(members).forEach(function (k) { var m = members[k]; m.ping = m.pingCount ? Math.round(m.pingSum / m.pingCount) : null; });
		// A link ends at a box that is not open, or at a row its group's filter shows: an open box's traffic is its rows'
		var shown = {};
		list.forEach(function (n) { n.shown.forEach(function (id) { shown[id] = true; }); });
		var end = function (id) { return nodes[id] ? !nodes[id].open : !!shown[id]; };
		var edgeList = Object.keys(edges).sort().map(function (k) { return edges[k]; }).filter(function (e) { return end(e.from) && end(e.to); });
		// Nodes nothing is going through and no server is behind (no web clients without a web server) stay out
		var linked = {};
		Object.keys(edges).forEach(function (k) { linked[edges[k].from] = linked[edges[k].to] = true; });
		list = list.filter(function (n) { return n.servers.length || n.open || linked[n.id] || (n.kind === 'clients' && n.connections > 0); });
		list.sort(function (a, b) { return a.column - b.column || a.order - b.order || String(a.label).localeCompare(String(b.label)); });
		return { nodes: list, edges: edgeList, members: members };
	}

	// Load of a link against its recent peak: 'idle', 'low', 'mid' or 'high'
	function loadClass(value, peak) {
		if (!(value > 0)) return 'idle';
		var ratio = peak > 0 ? value / peak : 1;
		return ratio >= 0.8 ? 'high' : ratio >= 0.4 ? 'mid' : 'low';
	}

	// Line width from bytes (or requests) per second: 1.5 px idle up to ~12 px
	function width(value) {
		return Math.round((1.5 + Math.min(10.5, Math.log10(1 + Math.max(0, value)) * 1.6)) * 10) / 10;
	}

	// Seconds one dash takes along the line: faster with more packets, still at nothing
	function speed(packets) {
		if (!(packets > 0)) return 0;
		return Math.round(Math.max(0.35, Math.min(4, 4 / Math.log10(10 + packets))) * 100) / 100;
	}

	// An open group's box height for `rows` members (at least one row, for "no matches" or "loading")
	function groupHeight(rows) {
		return GROUP.head + GROUP.filter + Math.max(1, Math.min(GROUP.maxRows, rows)) * GROUP.row + GROUP.foot;
	}

	/**
	 * Positions: columns across `width` (node centres), rows in each column spread over the tallest column's height.
	 * `moved` ({id: {fx, y}}, optional) places nodes the viewer dragged: fx is a fraction of the width, y in pixels.
	 * `scroll` ({groupId: px}, optional) is how far each open box's list is scrolled.
	 * Returns {width, height, nodeWidth, nodeHeight, nodes: {id: {x, y, h}}, lists: {groupId: {top, height, rows}},
	 * anchors: {memberId: {x, y}}, paths: {edgeId: {fwd, back}}}. A node's y is the centre of its header (of the box
	 * when closed), h its whole height; an open box's list starts at lists[id].top and anchors are the rows in view.
	 */
	function layout(graph, totalWidth, moved, scroll) {
		// Boxes take at most 60% of the space between column centres, so there is always room for the links between them
		var nodeWidth = Math.max(120, Math.min(230, Math.floor((totalWidth - 32) / 9))), nodeHeight = 68, rowHeight = 88, pad = 16;
		var byColumn = COLUMNS.map(function () { return []; });
		graph.nodes.forEach(function (n) { byColumn[n.column].push(n); });
		var rows = Math.max.apply(null, byColumn.map(function (c) { return c.length; }).concat([1]));
		var height = rows * rowHeight + pad * 2;
		var usable = totalWidth - nodeWidth - pad * 2;
		var xs = [0, 0.2, 0.4, 0.6, 0.8, 1].map(function (f) { return Math.round(pad + nodeWidth / 2 + f * usable); });
		var heightOf = function (n) { return n.open ? groupHeight(n.shown.length) : nodeHeight; };
		var at = {};
		// Rows at a fixed pitch from the top, each column centred on the tallest; an open box pushes the boxes under it
		// down by what it grew, so opening a group only moves the boxes of its own column
		byColumn.forEach(function (column, c) {
			var top = pad + (rows - column.length) * rowHeight / 2, extra = 0;
			column.forEach(function (n, i) {
				at[n.id] = { x: xs[c], y: Math.round(top + rowHeight * (i + 0.5) + extra), h: heightOf(n), column: c };
				extra += at[n.id].h - nodeHeight;
			});
		});
		moved = moved || {};
		scroll = scroll || {};
		var clampX = function (x) { return Math.round(Math.min(totalWidth - pad - nodeWidth / 2, Math.max(pad + nodeWidth / 2, x))); };
		// Boxes the viewer dragged
		graph.nodes.forEach(function (n) {
			var m = moved[n.id], a = at[n.id];
			if (!m || !(m.fx >= 0) || !(m.y >= 0)) return;
			a.x = clampX(m.fx * totalWidth);
			a.y = Math.round(Math.max(pad + nodeHeight / 2, m.y));
		});
		graph.nodes.forEach(function (n) { var a = at[n.id]; height = Math.max(height, a.y - nodeHeight / 2 + a.h + pad); });

		// An open box's rows in view of its list are link ends of their own, on the box's sides
		var lists = {}, anchors = {};
		graph.nodes.forEach(function (n) {
			if (!n.open) return;
			var a = at[n.id], top = a.y - nodeHeight / 2 + GROUP.head + GROUP.filter;
			var visible = Math.max(1, Math.min(GROUP.maxRows, n.shown.length)) * GROUP.row;
			var offset = Math.max(0, Math.min(num(scroll[n.id]), n.shown.length * GROUP.row - visible));
			lists[n.id] = { top: top, height: visible, rows: n.shown.length, scroll: offset };
			n.shown.forEach(function (id, i) {
				var y = top + (i + 0.5) * GROUP.row - offset;
				if (y < top || y > top + visible) return; // scrolled out of view: no links
				anchors[id] = { x: a.x, y: Math.round(y), h: GROUP.row, column: a.column, row: true };
			});
		});

		// Links leaving a box's side spread down that side, ordered by where their other end is, instead of all meeting
		// at one point (an open box's rows keep their own row)
		var ends = {}; // "id|side" -> [{edge, end, otherY}]
		var endOf = function (id) { return at[id] || anchors[id]; };
		graph.edges.forEach(function (e) {
			var a = endOf(e.from), b = endOf(e.to);
			if (!a || !b || Math.abs(b.x - a.x) < nodeWidth * 0.75) return;
			var dir = b.x > a.x ? 1 : -1;
			if (!a.row) (ends[e.from + '|' + dir] = ends[e.from + '|' + dir] || []).push({ edge: e.id, end: 'a', other: b.y });
			if (!b.row) (ends[e.to + '|' + (-dir)] = ends[e.to + '|' + (-dir)] || []).push({ edge: e.id, end: 'b', other: a.y });
		});
		var ports = {}; // edgeId -> {a: y, b: y}
		Object.keys(ends).forEach(function (key) {
			var list = ends[key], box = at[key.split('|')[0]];
			if (!box) return;
			list.sort(function (p, q) { return p.other - q.other; });
			var top = box.y - nodeHeight / 2 + 12, span = Math.min(box.h, nodeHeight) - 24;
			list.forEach(function (p, i) {
				var y = list.length === 1 ? box.y : top + span * i / (list.length - 1);
				(ports[p.edge] = ports[p.edge] || {})[p.end] = Math.round(y);
			});
		});
		var withPort = function (p, y) { return y === undefined ? p : { x: p.x, y: y, h: p.h, column: p.column, row: p.row, port: true }; };
		var paths = {};
		graph.edges.forEach(function (e) {
			var a = endOf(e.from), b = endOf(e.to);
			if (!a || !b) return;
			var pa = withPort(a, ports[e.id] && ports[e.id].a), pb = withPort(b, ports[e.id] && ports[e.id].b);
			paths[e.id] = { fwd: curve(pa, pb, nodeWidth, 3.5, nodeHeight), back: curve(pb, pa, nodeWidth, 3.5, nodeHeight) };
		});
		return { width: totalWidth, height: height, nodeWidth: nodeWidth, nodeHeight: nodeHeight, nodes: at, lists: lists, anchors: anchors, paths: paths };
	}

	// A curve between two boxes (or an open box's rows), leaving and entering on the sides that face each other
	// (left/right when they are apart sideways, top/bottom when one is above the other), moved `offset` px to its left so
	// the two directions sit side by side. Boxes in the same column with others between them, and rows of a box with
	// something in their own column, bulge out sideways instead of cutting through.
	function curve(a, b, nodeWidth, offset, nodeHeight) {
		var half = nodeWidth / 2, x1, y1, x2, y2, c1, c2;
		var halfA = (a.row ? a.h : nodeHeight || 54) / 2, halfB = (b.row ? b.h : nodeHeight || 54) / 2;
		var dx0 = b.x - a.x, dy0 = b.y - a.y;
		if (Math.abs(dx0) >= nodeWidth * 0.75) {
			var dir = dx0 > 0 ? 1 : -1;
			x1 = a.x + dir * half; x2 = b.x - dir * half; y1 = a.y; y2 = b.y;
			var mid = (x2 - x1) / 2;
			c1 = [x1 + mid, y1]; c2 = [x2 - mid, y2];
		} else if ((a.row || b.row) || (Math.abs(dy0) > nodeHeight * 2.2 && a.column === b.column && Math.abs(dx0) < 1)) {
			// Same column, far apart or from a row: out of the side with room and back in, clear of the boxes between
			var side = a.column === COLUMNS.length - 1 ? -1 : 1;
			x1 = a.x + side * half; x2 = b.x + side * half; y1 = a.y; y2 = b.y;
			var bulge = side * Math.max(50, Math.abs(dy0) * 0.35);
			c1 = [x1 + bulge, y1]; c2 = [x2 + bulge, y2];
		} else {
			// From the bottom of the upper box (an open one's too) to the top of the lower
			var nh = nodeHeight || 54;
			var top = function (p, half) { return p.row ? p.y - half : p.y - nh / 2; };
			var bottom = function (p, half) { return p.row ? p.y + half : p.y - nh / 2 + (p.h || nh); };
			x1 = a.x; x2 = b.x;
			if (dy0 >= 0) { y1 = bottom(a, halfA); y2 = top(b, halfB); } else { y1 = top(a, halfA); y2 = bottom(b, halfB); }
			var midY = (y2 - y1) / 2;
			c1 = [x1, y1 + midY]; c2 = [x2, y2 - midY];
		}
		// Offset along the normal of the chord
		var dx = x2 - x1, dy = y2 - y1, len = Math.sqrt(dx * dx + dy * dy) || 1;
		var nx = -dy / len * offset, ny = dx / len * offset;
		var p = function (x, y) { return (Math.round((x + nx) * 10) / 10) + ',' + (Math.round((y + ny) * 10) / 10); };
		return 'M' + p(x1, y1) + ' C' + p(c1[0], c1[1]) + ' ' + p(c2[0], c2[1]) + ' ' + p(x2, y2);
	}

	var api = { build: build, layout: layout, loadClass: loadClass, width: width, speed: speed, matches: matches, groupHeight: groupHeight, GROUP: GROUP };
	root.NetworkGraph = api;
})(typeof window !== 'undefined' ? window : globalThis);
