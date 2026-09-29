/**
 * The Network page's diagram model, without the DOM so it can be tested with node (tests/dWebTests/network-graph.test.mjs).
 *
 * build(summary, options) turns what /api/diagnostics/network (and the `traffic` WebSocket topic) sends into nodes and
 * links: game clients and web clients on the left, auth, chat, the worlds (one node per zone, or per instance when the
 * zone is expanded) and any other reporting server in the middle, master, the dashboard and the UGC server on the right.
 * Nodes come from whatever servers report, so a new kind of server shows up by itself. Each link has its rates each way:
 * `fwd` from `from` to `to`, `back` the other way, in packets (HTTP links: requests) and bytes per second; `exact` says
 * whether they are measured on that link or worked out from a server's totals (older servers report no split).
 *
 * layout(graph, width) places the nodes in four columns in a fixed order and gives each link its path.
 */
(function (root) {
	'use strict';

	var COLUMNS = [0, 1, 2, 3]; // clients | auth, chat, worlds, others | master | dashboard, UGC

	function num(v) { return typeof v === 'number' && isFinite(v) ? v : 0; }
	function rates(r) { r = r || {}; return { packets_in: num(r.packets_in), packets_out: num(r.packets_out), bytes_in: num(r.bytes_in), bytes_out: num(r.bytes_out) }; }
	function any(r) { return r.packets_in > 0 || r.packets_out > 0 || r.bytes_in > 0 || r.bytes_out > 0; }

	// The node a server belongs to
	function nodeOf(server, expanded) {
		switch (server.type) {
			case 'AUTH': return { id: 'auth', kind: 'auth', label: 'Auth', column: 1, order: 0 };
			case 'CHAT': return { id: 'chat', kind: 'chat', label: 'Chat', column: 1, order: 1 };
			case 'MASTER': return { id: 'master', kind: 'master', label: 'Master', column: 2, order: 0 };
			case 'DASHBOARD': return { id: 'dashboard', kind: 'dashboard', label: 'Dashboard', column: 3, order: 0 };
			case 'UGC': return { id: 'ugc', kind: 'ugc', label: 'UGC server', column: 3, order: 1 };
			case 'WORLD': {
				var zone = num(server.zone);
				// "World 1200 Nimbus Station #3" -> "World 1200 Nimbus Station"
				var zoneLabel = String(server.label || ('World ' + zone)).replace(/\s+#\d+$/, '');
				if (expanded && expanded[zone]) return { id: server.key, kind: 'world', label: server.label || server.key, column: 1, order: 2 + zone + num(server.instance) / 1e6, zone: zone };
				return { id: 'zone:' + zone, kind: 'zone', label: zoneLabel, column: 1, order: 2 + zone, zone: zone };
			}
			default: return { id: server.key, kind: 'other', label: server.label || server.key, column: 1, order: 1e9 };
		}
	}

	function build(summary, options) {
		options = options || {};
		var servers = (summary && summary.servers) || {};
		var keys = Object.keys(servers).sort();
		var nodes = {}, edges = {};

		function node(spec) {
			var n = nodes[spec.id];
			if (!n) {
				n = nodes[spec.id] = { id: spec.id, kind: spec.kind, label: spec.label, column: spec.column, order: spec.order, zone: spec.zone, servers: [], instances: 0,
					connections: 0, pingSum: 0, pingCount: 0, resends: 0, resendQueue: 0, workersBusy: 0, workersThreads: 0, workersQueued: 0, websockets: 0,
					packetsIn: 0, packetsOut: 0, bytesIn: 0, bytesOut: 0, http: 0, noSplit: false };
			}
			return n;
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

		var clients = node({ id: 'clients', kind: 'clients', label: 'Game clients', column: 0, order: 0 });
		var web = node({ id: 'web', kind: 'web', label: 'Web clients', column: 0, order: 10 });
		var hasChat = keys.some(function (k) { return servers[k].type === 'CHAT'; });

		keys.forEach(function (key) {
			var s = servers[key];
			var n = node(nodeOf(s, options.expanded));
			var link = s.link || {}, gauges = s.gauges || {};
			n.servers.push(key);
			if (s.type === 'WORLD') n.instances++;
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
					if (any(m)) edge(n.id, 'master', m.packets_out, m.bytes_out, m.packets_in, m.bytes_in, true);
				} else if (s.type === 'DASHBOARD' || s.type === 'UGC' || s.type === 'CHAT') {
					// Nearly all of their packets are with master (chat's are with the worlds too)
					edge(n.id, 'master', totals.packets_out, totals.bytes_out, totals.packets_in, totals.bytes_in, false, 'packets', ESTIMATED);
				}
			}

			if (s.type === 'DASHBOARD' || s.type === 'UGC') {
				// HTTP: requests from browsers and game clients (all but other servers'), and the bodies answered
				var fromServers = split ? num(split.http_from_servers) : 0, fromServersBytes = split ? num(split.http_from_servers_bytes_out) : 0;
				var requests = Math.max(0, num(s.http) - fromServers), bytesOut = Math.max(0, num(s.http_bytes_out) - fromServersBytes);
				var who = s.type === 'UGC' ? 'clients' : 'web';
				if (requests > 0 || bytesOut > 0) edge(who, n.id, requests, null, requests, bytesOut, !!split, 'http', split ? '' : 'other servers\' requests not told apart (older server)');
				// The dashboard's own requests to the UGC server, counted where they were made
				if (split && num(split.http_out) > 0) edge(n.id, 'ugc', split.http_out, null, split.http_out, split.http_out_bytes_in, true, 'http');
				return;
			}
			if (s.type === 'MASTER') return;

			if (split) {
				var c = rates(split.clients);
				if (any(c)) edge('clients', n.id, c.packets_in, c.bytes_in, c.packets_out, c.bytes_out, true);
				var o = rates(split.servers);
				// A world's "other servers" is its chat link; chat's are the worlds, drawn from their side
				if (s.type === 'WORLD' && any(o)) edge(n.id, hasChat ? 'chat' : 'master', o.packets_out, o.bytes_out, o.packets_in, o.bytes_in, hasChat, 'packets', hasChat ? '' : 'chat link, chat not reporting');
				else if (s.type !== 'CHAT' && any(o)) edge(n.id, 'master', o.packets_out, o.bytes_out, o.packets_in, o.bytes_in, false, 'servers', 'with other servers, drawn to master');
			} else if (s.type !== 'CHAT') {
				edge('clients', n.id, totals.packets_in, totals.bytes_in, totals.packets_out, totals.bytes_out, false, 'packets', ESTIMATED);
			}
		});

		// Game clients and web clients: connections from the servers they use
		keys.forEach(function (key) {
			var s = servers[key], link = s.link || {};
			if (s.type === 'AUTH' || s.type === 'WORLD') clients.connections += Math.max(0, num(link.connections) - 1); // less its master link
		});
		web.websockets = keys.reduce(function (t, k) { return t + (servers[k].type === 'DASHBOARD' ? num((servers[k].gauges || {}).websocket_clients) : 0); }, 0);

		// Single clients (from /api/diagnostics/network/connections) when their node is expanded
		(options.peers || []).forEach(function (peer, i) {
			if (!(peer.kind === 'game' && options.clientsExpanded) && !(peer.kind === 'web' && options.webExpanded)) return;
			var limit = options.peerLimit || 8;
			var shownOfKind = (options.peers || []).slice(0, i).filter(function (p) { return p.kind === peer.kind; }).length;
			if (shownOfKind >= limit) return;
			var id = 'peer:' + peer.address;
			var who = peer.character || peer.account || peer.address;
			var p = node({ id: id, kind: 'peer', label: who, column: 0, order: (peer.kind === 'game' ? 0.1 : 10.1) + shownOfKind / 100 });
			p.peer = peer;
			(peer.servers || []).forEach(function (entry) {
				var target = servers[entry.server];
				if (!target) return;
				var to = nodeOf(target, options.expanded).id;
				if (!nodes[to]) return;
				if (entry.http) edge(id, to, entry.packets_in, entry.bytes_in, entry.packets_out, entry.bytes_out, true, 'peer-http');
				else edge(id, to, entry.packets_in, entry.bytes_in, entry.packets_out, entry.bytes_out, true, 'peer', 'RakNet datagrams of this connection, acknowledgements and resends included');
				if (!entry.http) { p.connections++; p.pingSum += num(entry.ping_ms); p.pingCount++; p.resends += num(entry.resends); }
			});
		});

		var list = Object.keys(nodes).map(function (k) { return nodes[k]; });
		list.forEach(function (n) { n.ping = n.pingCount ? Math.round(n.pingSum / n.pingCount) : null; });
		// Nodes nothing is going through and no server is behind (no web clients without a web server) stay out
		var linked = {};
		Object.keys(edges).forEach(function (k) { linked[edges[k].from] = linked[edges[k].to] = true; });
		list = list.filter(function (n) { return n.servers.length || linked[n.id] || (n.kind === 'clients' && n.connections > 0); });
		list.sort(function (a, b) { return a.column - b.column || a.order - b.order || String(a.label).localeCompare(String(b.label)); });
		var edgeList = Object.keys(edges).sort().map(function (k) { return edges[k]; }).filter(function (e) { return nodes[e.from] && nodes[e.to]; });
		return { nodes: list, edges: edgeList };
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

	/**
	 * Positions: columns across `width` (node centres), rows in each column spread over the tallest column's height.
	 * Returns {width, height, nodeWidth, nodeHeight, nodes: {id: {x, y}}, paths: {edgeId: {fwd, back}}}.
	 */
	function layout(graph, totalWidth) {
		var nodeWidth = Math.max(110, Math.min(170, Math.floor(totalWidth / 5.2))), nodeHeight = 54, rowHeight = 74, pad = 16;
		var byColumn = COLUMNS.map(function () { return []; });
		graph.nodes.forEach(function (n) { byColumn[n.column].push(n); });
		var rows = Math.max.apply(null, byColumn.map(function (c) { return c.length; }).concat([1]));
		var height = rows * rowHeight + pad * 2;
		var usable = totalWidth - nodeWidth - pad * 2;
		var xs = [0, 0.36, 0.68, 1].map(function (f) { return Math.round(pad + nodeWidth / 2 + f * usable); });
		var at = {};
		byColumn.forEach(function (column, c) {
			var step = (height - pad * 2) / Math.max(column.length, 1);
			column.forEach(function (n, i) { at[n.id] = { x: xs[c], y: Math.round(pad + step * (i + 0.5)), column: c }; });
		});
		var paths = {};
		graph.edges.forEach(function (e) {
			var a = at[e.from], b = at[e.to];
			if (!a || !b) return;
			paths[e.id] = { fwd: curve(a, b, nodeWidth, 3.5), back: curve(b, a, nodeWidth, 3.5) };
		});
		return { width: totalWidth, height: height, nodeWidth: nodeWidth, nodeHeight: nodeHeight, nodes: at, paths: paths };
	}

	// A curve from a's side to b's side, moved `offset` px to its left (so the two directions sit side by side)
	function curve(a, b, nodeWidth, offset) {
		var half = nodeWidth / 2, x1, x2, c1, c2;
		if (a.x === b.x) {
			// Same column: bulge out on the side with room (left of the rightmost column, right of the others)
			var side = a.column === COLUMNS.length - 1 ? -1 : 1;
			x1 = x2 = a.x + side * half;
			var bulge = side * Math.max(50, Math.abs(b.y - a.y) * 0.35);
			c1 = [x1 + bulge, a.y]; c2 = [x2 + bulge, b.y];
		} else {
			var dir = b.x > a.x ? 1 : -1;
			x1 = a.x + dir * half; x2 = b.x - dir * half;
			var mid = (x2 - x1) / 2;
			c1 = [x1 + mid, a.y]; c2 = [x2 - mid, b.y];
		}
		// Offset along the normal of the chord
		var dx = x2 - x1, dy = b.y - a.y, len = Math.sqrt(dx * dx + dy * dy) || 1;
		var nx = -dy / len * offset, ny = dx / len * offset;
		var p = function (x, y) { return (Math.round((x + nx) * 10) / 10) + ',' + (Math.round((y + ny) * 10) / 10); };
		return 'M' + p(x1, a.y) + ' C' + p(c1[0], c1[1]) + ' ' + p(c2[0], c2[1]) + ' ' + p(x2, b.y);
	}

	var api = { build: build, layout: layout, loadClass: loadClass, width: width, speed: speed };
	root.NetworkGraph = api;
})(typeof window !== 'undefined' ? window : globalThis);
