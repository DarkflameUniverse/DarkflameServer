// The Network page's diagram model (static/js/network-graph.js): nodes and links from the `traffic` summary.
// Run by ctest: node network-graph.test.mjs <network-graph.js>
import { readFileSync } from 'node:fs';
import vm from 'node:vm';

const [scriptPath] = process.argv.slice(2);
const context = { window: {} };
vm.createContext(context);
vm.runInContext(readFileSync(scriptPath, 'utf8'), context);
const G = context.window.NetworkGraph;
let failures = 0;
const same = (actual, expected, what) => {
	if (JSON.stringify(actual) !== JSON.stringify(expected)) {
		failures++;
		console.error(`${what}: ${JSON.stringify(actual)} is not ${JSON.stringify(expected)}`);
	}
};
const r = (pi, po, bi, bo) => ({ packets_in: pi, packets_out: po, bytes_in: bi, bytes_out: bo });
const split = (clients, master, servers, http = {}) => Object.assign({ clients, master, servers, http_from_servers: 0, http_from_servers_bytes_out: 0, http_out: 0, http_out_bytes_in: 0 }, http);

// What NetworkView::Summary sends
const summary = {
	time: 1700000000,
	servers: {
		master: Object.assign(r(30, 30, 3000, 3000), { key: 'master', type: 'MASTER', label: 'Master', link: { connections: 5 }, split: split(r(0, 0, 0, 0), r(0, 0, 0, 0), r(30, 30, 3000, 3000)) }),
		auth: Object.assign(r(4, 4, 400, 400), { key: 'auth', type: 'AUTH', label: 'Auth', link: { connections: 3, ping_ms: 20 }, split: split(r(3, 3, 300, 300), r(1, 1, 100, 100), r(0, 0, 0, 0)) }),
		chat: Object.assign(r(2, 2, 200, 200), { key: 'chat', type: 'CHAT', label: 'Chat', link: { connections: 2 }, split: split(r(0, 0, 0, 0), r(1, 1, 100, 100), r(1, 1, 100, 100)) }),
		'world:1200:1': Object.assign(r(20, 40, 2000, 8000), { key: 'world:1200:1', type: 'WORLD', zone: 1200, instance: 1, label: 'World 1200 Nimbus Station #1', link: { connections: 11, ping_ms: 50, resends: 3 },
			split: split(r(18, 38, 1800, 7800), r(1, 1, 100, 100), r(1, 1, 100, 100)) }),
		'world:1200:2': Object.assign(r(10, 20, 1000, 4000), { key: 'world:1200:2', type: 'WORLD', zone: 1200, instance: 2, label: 'World 1200 Nimbus Station #2', link: { connections: 6, ping_ms: 70 },
			split: split(r(9, 19, 900, 3900), r(1, 1, 100, 100), r(0, 0, 0, 0)) }),
		dashboard: Object.assign(r(1, 1, 100, 100), { key: 'dashboard', type: 'DASHBOARD', label: 'Dashboard', http: 4, http_bytes_out: 40000, link: { connections: 1 }, gauges: { websocket_clients: 2, workers_busy: 1, workers_threads: 4 },
			split: split(r(0, 0, 0, 0), r(1, 1, 100, 100), r(0, 0, 0, 0), { http_out: 1, http_out_bytes_in: 5000 }) }),
		ugc: Object.assign(r(1, 1, 100, 100), { key: 'ugc', type: 'UGC', label: 'UGC', http: 3, http_bytes_out: 70000, link: { connections: 1 },
			split: split(r(0, 0, 0, 0), r(1, 1, 100, 100), r(0, 0, 0, 0), { http_from_servers: 1, http_from_servers_bytes_out: 5000 }) }),
		'service:99:0:0': Object.assign(r(1, 1, 10, 10), { key: 'service:99:0:0', type: '', label: 'service:99:0:0', link: {}, split: null })
	}
};

const graph = G.build(summary, {});
const ids = graph.nodes.map((n) => n.id);
same(ids, ['clients', 'auth', 'chat', 'zone:1200', 'service:99:0:0', 'master', 'dashboard', 'ugc', 'web'], 'nodes in column order, one per zone');
const edge = (id) => graph.edges.find((e) => e.id === id);

same(edge('clients>zone:1200').fwd, { packets: 27, bytes: 2700 }, 'players to the zone: both instances');
same(edge('clients>zone:1200').back, { packets: 57, bytes: 11700 }, 'the zone to its players');
same(edge('clients>zone:1200').exact, true, 'measured');
same(edge('zone:1200>master').fwd, { packets: 2, bytes: 200 }, 'the zone to master');
same(edge('zone:1200>chat').fwd, { packets: 1, bytes: 100 }, 'the zone to chat');
same(edge('clients>auth').fwd.packets, 3, 'players to auth');
same(edge('web>dashboard:http').fwd.packets, 4, 'browser requests to the dashboard');
same(edge('web>dashboard:http').back.bytes, 40000, 'what the dashboard answered them');
same(edge('clients>ugc:http').fwd.packets, 2, 'game clients\' requests to UGC, less the dashboard\'s');
same(edge('clients>ugc:http').back.bytes, 65000, 'UGC files sent to them');
same(edge('dashboard>ugc:http').back.bytes, 5000, 'the dashboard fetching from UGC');
same(edge('dashboard>master').exact, true, 'dashboard master link measured');
same(edge('clients>service:99:0:0').exact, false, 'a server without a split: estimated from totals');
same(graph.nodes.find((n) => n.id === 'service:99:0:0').noSplit, true, 'and marked');
same(graph.nodes.find((n) => n.id === 'clients').connections, 2 + 10 + 5, 'players: connections of auth and worlds less their master links');
same(graph.nodes.find((n) => n.id === 'web').websockets, 2, 'live dashboard pages');
same(graph.nodes.find((n) => n.id === 'zone:1200').instances, 2, 'instances');
same(graph.nodes.find((n) => n.id === 'zone:1200').label, 'World 1200 Nimbus Station', 'zone label without the instance');

// Expanding the zone draws each instance
const open = G.build(summary, { expanded: { 1200: true } });
same(open.nodes.filter((n) => n.kind === 'world').map((n) => n.id), ['world:1200:1', 'world:1200:2'], 'instances when expanded');
same(open.edges.find((e) => e.id === 'clients>world:1200:2').fwd.packets, 9, 'an instance\'s own link');

// Single clients from the connections list
const peers = [{ address: 'peer-0001', kind: 'game', character: 'Alice', servers: [{ server: 'world:1200:1', packets_in: 5, packets_out: 9, bytes_in: 500, bytes_out: 900, ping_ms: 40 }] },
	{ address: 'peer-0002', kind: 'web', servers: [{ server: 'dashboard', http: true, packets_in: 1, packets_out: 1, bytes_in: 200, bytes_out: 3000 }] }];
const withPeers = G.build(summary, { clientsExpanded: true, peers });
same(withPeers.nodes.filter((n) => n.kind === 'peer').map((n) => n.label), ['Alice'], 'only game clients when only they are expanded');
same(withPeers.edges.find((e) => e.id === 'peer:peer-0001>zone:1200:peer').back.bytes, 900, 'a client\'s own link');

// No servers: nothing to draw but nodes with nothing behind them are left out
same(G.build({ servers: {} }, {}).nodes.length, 0, 'empty summary');

// Load colours, widths and speeds
same([G.loadClass(0, 10), G.loadClass(1, 10), G.loadClass(5, 10), G.loadClass(9, 10), G.loadClass(5, 0)], ['idle', 'low', 'mid', 'high', 'high'], 'load classes');
same(G.width(0), 1.5, 'idle width');
same(G.width(1e9) <= 12, true, 'width is capped');
same(G.speed(0), 0, 'no packets, no motion');
same(G.speed(1e6) < G.speed(1), true, 'busier moves faster');

// Layout: columns left to right, every link a path each way
const L = G.layout(graph, 1000);
same(L.nodes.clients.x < L.nodes.auth.x && L.nodes.auth.x < L.nodes.master.x && L.nodes.master.x < L.nodes.dashboard.x && L.nodes.dashboard.x < L.nodes.web.x, true, 'columns, web clients right of the dashboard');
same(graph.edges.every((e) => L.paths[e.id] && /^M/.test(L.paths[e.id].fwd) && /^M/.test(L.paths[e.id].back)), true, 'paths');
same(G.layout(graph, 1000), L, 'the same layout every time');

if (failures) {
	console.error(`${failures} failure(s)`);
	process.exit(1);
}
console.log('network-graph: all passed');
