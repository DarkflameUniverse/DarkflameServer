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
same(ids, ['clients', 'auth', 'zone:1200', 'service:99:0:0', 'chat', 'master', 'dashboard', 'ugc', 'web'], 'nodes in column order, one per zone, chat in its own column');
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

// Opening the zone: its instances are rows of its box, each with its own links, and the box has none of its own
const open = G.build(summary, { expanded: { 1200: true } });
const zoneBox = open.nodes.find((n) => n.id === 'zone:1200');
same(zoneBox.open, true, 'the zone is open');
same(zoneBox.members.map((m) => m.id), ['world:1200:1', 'world:1200:2'], 'its instances are its rows, in order');
same(zoneBox.shown, ['world:1200:1', 'world:1200:2'], 'all shown without a filter');
same(open.nodes.filter((n) => n.kind === 'world').length, 0, 'no box per instance');
same(open.edges.find((e) => e.id === 'clients>world:1200:2').fwd.packets, 9, 'an instance\'s own link');
same(open.edges.find((e) => e.id === 'world:1200:1>chat').fwd.packets, 1, 'an instance\'s chat link');
same(open.edges.some((e) => e.from === 'zone:1200' || e.to === 'zone:1200'), false, 'no links from the open zone\'s header');
same(open.members['world:1200:2'].label, '#2', 'a row is its instance');
same(G.build(summary, { expanded: { 1200: true }, filters: { 'zone:1200': '#2' } }).nodes.find((n) => n.id === 'zone:1200').shown, ['world:1200:2'], 'filtered by instance');

// Single clients from the connections list
const peers = [{ address: 'peer-0001', kind: 'game', account: 'alice', character: 'Alice', servers: [{ server: 'world:1200:1', label: 'World 1200 Nimbus Station #1', packets_in: 5, packets_out: 9, bytes_in: 500, bytes_out: 900, ping_ms: 40 }] },
	{ address: '198.51.100.7', kind: 'web', account_id: 7, user: 'mod-alice', servers: [{ server: 'dashboard', http: true, packets_in: 1, packets_out: 1, bytes_in: 200, bytes_out: 3000 }] },
	{ address: '198.51.100.7', kind: 'web', account_id: 9, user: 'mod-bob', servers: [{ server: 'dashboard', http: true, packets_in: 2, packets_out: 2, bytes_in: 100, bytes_out: 1000 }] },
	{ address: '203.0.113.9', kind: 'web', servers: [{ server: 'ugc', http: true, packets_in: 1, packets_out: 1, bytes_in: 10, bytes_out: 5000 }] }];
const withPeers = G.build(summary, { clientsExpanded: true, peers });
const clientsBox = withPeers.nodes.find((n) => n.id === 'clients');
same(clientsBox.open, true, 'game clients open');
same(clientsBox.members.map((m) => m.label), ['203.0.113.9', 'Alice'], 'players, and HTTP clients of the UGC server only, are game clients');
same(withPeers.nodes.find((n) => n.id === 'web').open, false, 'web clients stay closed');
same(withPeers.edges.find((e) => e.id === 'peer:clients:peer-0001>zone:1200:peer').back.bytes, 900, 'a client\'s own link');
same(withPeers.edges.some((e) => e.from === 'clients' || e.to === 'clients'), false, 'no summed links from the open header');
same(withPeers.edges.some((e) => e.id === 'web>dashboard:http'), true, 'the closed web clients keep theirs');

// Game clients open while the list is loading: the box keeps its summed links
same(G.build(summary, { clientsExpanded: true }).edges.some((e) => e.from === 'clients'), true, 'summed links until the list is there');

// Web clients: a row per signed-in dashboard user, even on one address, filtered by name, user and (only when shown) address
const webOpen = G.build(summary, { webExpanded: true, peers });
same(webOpen.nodes.find((n) => n.id === 'web').members.map((m) => m.label), ['mod-alice', 'mod-bob'], 'one row per user');
same(webOpen.edges.filter((e) => e.from.startsWith('peer:web:')).map((e) => e.fwd.packets), [1, 2], 'each user\'s own requests');
same(G.build(summary, { webExpanded: true, peers, filters: { web: 'BOB' } }).nodes.find((n) => n.id === 'web').shown.length, 1, 'filter by user, any case');
same(G.build(summary, { webExpanded: true, peers, filters: { web: 'bob' } }).edges.filter((e) => e.from.startsWith('peer:web:')).length, 1, 'only the shown rows have links');
same(G.build(summary, { webExpanded: true, peers, filters: { web: '198.51' } }).nodes.find((n) => n.id === 'web').shown.length, 0, 'addresses are not searched without network_ips');
same(G.build(summary, { webExpanded: true, peers, addressesShown: true, filters: { web: '198.51' } }).nodes.find((n) => n.id === 'web').shown.length, 2, 'and are with it');
same(G.build(summary, { clientsExpanded: true, peers, filters: { clients: 'nimbus' } }).nodes.find((n) => n.id === 'clients').shown.length, 1, 'filter by the instance a player is on');
same(G.matches({ search: 'alice nimbus station' }, 'nim ali'), true, 'every word matches');

// No servers: nothing to draw but nodes with nothing behind them are left out
same(G.build({ servers: {} }, {}).nodes.length, 0, 'empty summary');

// Load colours, widths and speeds
same([G.loadClass(0, 10), G.loadClass(1, 10), G.loadClass(5, 10), G.loadClass(9, 10), G.loadClass(5, 0)], ['idle', 'low', 'mid', 'high', 'high'], 'load classes');
same(G.width(0), 1.5, 'idle width');
same(G.width(1e9) <= 12, true, 'width is capped');
same(G.speed(0), 0, 'no packets, no motion');
same(G.speed(1e6) < G.speed(1), true, 'busier moves faster');

// Layout: columns left to right, every link a path each way (the lane offset moves an end by up to 3.5 px)
const L = G.layout(graph, 1000);
same(L.nodes.clients.x < L.nodes.auth.x && L.nodes.auth.x < L.nodes.chat.x && L.nodes.chat.x < L.nodes.master.x && L.nodes.master.x < L.nodes.dashboard.x && L.nodes.dashboard.x < L.nodes.web.x, true, 'columns, web clients right of the dashboard');

// A dragged box goes where it was put, its links follow, and the box grows the canvas if needed
{
	const M = G.layout(graph, 1000, { master: { fx: 0.5, y: 900 } });
	same(M.nodes.master.x === 500 && M.nodes.master.y === 900, true, 'dragged node placed');
	same(M.height >= 900 + M.nodeHeight / 2, true, 'canvas grows for a dragged node');
	same(Object.keys(M.paths).length === Object.keys(L.paths).length, true, 'links still drawn');
}
same(graph.edges.every((e) => L.paths[e.id] && /^M/.test(L.paths[e.id].fwd) && /^M/.test(L.paths[e.id].back)), true, 'paths');
same(G.layout(graph, 1000), L, 'the same layout every time');

// Open groups grow by their rows, and their rows are the link ends
{
	const many = peers.concat(Array.from({ length: 10 }, (_, i) => ({ address: 'peer-1' + i, kind: 'web', account_id: 100 + i, user: 'user' + String(i).padStart(2, '0'),
		servers: [{ server: 'dashboard', http: true, packets_in: 1, packets_out: 1, bytes_in: 1, bytes_out: 1 }] })));
	const g = G.build(summary, { webExpanded: true, peers: many });
	const box = g.nodes.find((n) => n.id === 'web');
	same(box.shown.length, 12, 'twelve web users');
	const O = G.layout(g, 1000);
	const closedHeight = L.nodes.web.h;
	same(closedHeight, L.nodeHeight, 'a closed box is its usual height');
	same(O.nodes.web.h, G.groupHeight(12), 'the open box grows');
	same(G.groupHeight(12), G.groupHeight(8), 'up to eight rows, the rest scroll');
	same(G.groupHeight(3) > G.groupHeight(2) && G.groupHeight(2) > closedHeight, true, 'taller with more rows');
	same(G.layout(G.build(summary, { webExpanded: true, peers: many, filters: { web: 'mod-' } }), 1000).nodes.web.h, G.groupHeight(2), 'shrinks to what the filter shows');
	const inView = box.shown.filter((id) => O.anchors[id]);
	same(inView, box.shown.slice(0, 8), 'the first eight rows are anchors');
	same(O.anchors[box.shown[1]].y - O.anchors[box.shown[0]].y, G.GROUP.row, 'a row apart');
	same(O.anchors[box.shown[0]].y > O.nodes.web.y && O.anchors[box.shown[0]].y > O.lists.web.top, true, 'rows are under the header and the filter');
	same(Object.keys(O.paths).some((id) => id.startsWith('web>') || id.includes('>web')), false, 'no links from the open header');
	const rowEdge = g.edges.find((e) => e.from === box.shown[0]);
	const startX = Number(/^M([\d.]+),/.exec(O.paths[rowEdge.id].fwd)[1]);
	same(Math.abs(startX - (O.nodes.web.x - O.nodeWidth / 2)) <= 3.5, true, 'a row\'s link leaves from the box\'s side');
	same(g.edges.filter((e) => e.from === box.shown[10]).every((e) => !O.paths[e.id]), true, 'rows scrolled out of view have no links');
	const S = G.layout(g, 1000, {}, { web: 4 * G.GROUP.row });
	same(box.shown.filter((id) => S.anchors[id]), box.shown.slice(4, 12), 'scrolled by four rows');
	same(G.layout(g, 1000, {}, { web: 1e6 }).lists.web.scroll, 4 * G.GROUP.row, 'scrolling stops at the end');
	same(O.nodes.dashboard, L.nodes.dashboard, 'other columns stay put');
}
{
	// An open zone pushes the boxes under it down by what it grew
	const Z = G.layout(G.build(summary, { expanded: { 1200: true } }), 1000);
	same(Z.nodes['service:99:0:0'].y - L.nodes['service:99:0:0'].y, G.groupHeight(2) - L.nodeHeight, 'the box under the zone moves down');
	same(Z.nodes.auth.y, L.nodes.auth.y, 'the boxes above stay');
	same(Z.height >= Z.nodes['zone:1200'].y - Z.nodeHeight / 2 + Z.nodes['zone:1200'].h, true, 'the canvas fits the open box');
	same(['world:1200:1', 'world:1200:2'].every((id) => Z.anchors[id]), true, 'instance rows are anchors');
	same(Z.paths['clients>world:1200:1'] && Z.paths['world:1200:1>chat'] ? true : false, true, 'instance rows are linked on both sides');
}

// Listening ports: on a server's box, and on an open zone's instance rows; remote ports only when the list has them
{
	const withPorts = JSON.parse(JSON.stringify(summary));
	withPorts.servers.auth.port = 1001;
	withPorts.servers['world:1200:1'].port = 3015;
	withPorts.servers['world:1200:2'].port = 3016;
	same(G.portLabel(1001), ':1001', 'port label');
	same(G.portLabel(null), '', 'no port, no label');
	const g = G.build(withPorts, {});
	same(G.nameOf(g.nodes.find((n) => n.id === 'auth')), 'Auth :1001', 'auth box with its port');
	same(g.nodes.find((n) => n.id === 'zone:1200').port, null, 'a zone of two instances has no one port');
	same(G.nameOf(g.nodes.find((n) => n.id === 'chat')), 'Chat', 'no port known: just the name');
	const one = JSON.parse(JSON.stringify(withPorts));
	delete one.servers['world:1200:2'];
	same(G.nameOf(G.build(one, {}).nodes.find((n) => n.id === 'zone:1200')), 'World 1200 Nimbus Station :3015', 'a zone of one instance has its port');
	const open = G.build(withPorts, { expanded: { 1200: true } });
	same(open.members['world:1200:2'].port, 3016, 'an instance row has its port');
	same(G.peerAddress({ address: '203.0.113.5', servers: [{ port: 51234 }, { http: true, port: 80 }] }), '203.0.113.5:51234', 'a player\'s remote port');
	same(G.peerAddress({ address: 'peer-0001', servers: [{ port: null }] }), 'peer-0001', 'no remote port without network_ips');
	same(G.build(withPorts, { clientsExpanded: true, addressesShown: true, peers: [{ address: '203.0.113.5', kind: 'game', account: 'x', servers: [{ server: 'auth', port: 51234 }] }],
		filters: { clients: '51234' } }).nodes.find((n) => n.id === 'clients').shown.length, 1, 'filter by remote port with network_ips');
}

// One machine: no frames, nothing marked, the same layout as without machines
{
	const same1 = JSON.parse(JSON.stringify(summary));
	Object.values(same1.servers).forEach((s) => { s.host = 'peer-aaaa'; });
	const g = G.build(same1, {});
	same(g.hosts, [], 'one machine: no hosts');
	same(g.edges.some((e) => e.cross), false, 'no link crosses machines');
	same(g.nodes.map((n) => n.id), ids, 'the same boxes');
	const L1 = G.layout(g, 1000);
	same(L1.frames, {}, 'no frames');
	same(L1.nodes, G.layout(graph, 1000).nodes, 'the same places');
}

// Two machines: the second world, chat and UGC on another; frames, totals and marked links
{
	const two = JSON.parse(JSON.stringify(summary));
	Object.values(two.servers).forEach((s) => { s.host = 'peer-aaaa'; });
	two.servers['world:1200:2'].host = 'peer-bbbb';
	two.servers.chat.host = 'peer-bbbb';
	delete two.servers['service:99:0:0'];
	const g = G.build(two, { hostNames: { 'peer-bbbb': '198.51.100.20' } });
	same(g.hosts.map((h) => h.id), ['peer-aaaa', 'peer-bbbb'], 'two machines, master\'s first');
	same(g.hosts[0].master, true, 'master\'s machine is marked');
	same(g.hosts[1].label, '198.51.100.20', 'an address when the viewer may see it');
	same(g.hosts[0].label, 'peer-aaaa', 'else its token');
	same(g.hosts[1].servers, 2, 'servers on the second machine');
	same(g.hosts[1].connections, 6 + 2, 'its connections');
	same(g.hosts[1].packetsIn, 10 + 2, 'its packets');
	same(g.nodes.some((n) => n.id === 'zone:1200@peer-aaaa') && g.nodes.some((n) => n.id === 'zone:1200@peer-bbbb'), true, 'a zone on both machines is a box on each');
	same(g.hosts[1].nodes.sort(), ['chat', 'zone:1200@peer-bbbb'], 'the second machine\'s boxes');
	same(g.edges.find((e) => e.id === 'zone:1200@peer-aaaa>chat').cross, true, 'a world to chat on another machine is marked');
	same(g.edges.find((e) => e.id === 'zone:1200@peer-bbbb>master').cross, true, 'the other machine\'s world to master too');
	same(g.edges.find((e) => e.id === 'zone:1200@peer-aaaa>master').cross, false, 'on the same machine it is not');
	same(g.edges.find((e) => e.id === 'auth>master').cross, false, 'auth and master share a machine');
	same(g.edges.find((e) => e.id === 'clients>auth').cross, false, 'players are no machine');
	const L2 = G.layout(g, 1000);
	same(Object.keys(L2.frames), ['peer-aaaa', 'peer-bbbb'], 'a frame per machine');
	const fa = L2.frames['peer-aaaa'], fb = L2.frames['peer-bbbb'];
	same(fa.y + fa.h <= fb.y, true, 'the frames do not overlap: one under the other');
	const inside = (f, id) => { const a = L2.nodes[id]; return a.x - L2.nodeWidth / 2 >= f.x && a.x + L2.nodeWidth / 2 <= f.x + f.w && a.y - L2.nodeHeight / 2 >= f.y && a.y - L2.nodeHeight / 2 + a.h <= f.y + f.h; };
	same(['auth', 'master', 'dashboard', 'ugc', 'zone:1200@peer-aaaa'].every((id) => inside(fa, id)), true, 'master\'s machine frames its boxes');
	same(['chat', 'zone:1200@peer-bbbb'].every((id) => inside(fb, id)), true, 'and the other its own');
	same(L2.paths['zone:1200@peer-aaaa>chat'].mid && typeof L2.paths['zone:1200@peer-aaaa>chat'].mid.x === 'number', true, 'a crossing link has a mark half way');
	same(L2.paths['auth>master'].mid, undefined, 'others have none');
	// A dragged box takes its frame along
	const D = G.layout(g, 1000, { chat: { fx: 0.5, y: 1500 } });
	same(D.frames['peer-bbbb'].y + D.frames['peer-bbbb'].h >= 1500 + D.nodeHeight / 2, true, 'the frame follows a dragged box');
	// Collapsed: the machine is one box, its links go to it, links inside it are gone
	const c = G.build(two, { collapsedHosts: { 'peer-bbbb': true } });
	same(c.nodes.some((n) => n.id === 'chat'), false, 'the collapsed machine\'s boxes are gone');
	const hostBox = c.nodes.find((n) => n.id === 'host:peer-bbbb');
	same(hostBox && hostBox.kind, 'host', 'one box for the machine');
	same(c.edges.find((e) => e.id === 'zone:1200@peer-aaaa>host:peer-bbbb').cross, true, 'links into it are marked');
	same(c.edges.some((e) => e.from === e.to), false, 'links inside it are gone');
	same(c.hosts[1].collapsed, true, 'collapsed');
	same(Object.keys(G.layout(c, 1000).frames).length, 2, 'still framed');
}

if (failures) {
	console.error(`${failures} failure(s)`);
	process.exit(1);
}
console.log('network-graph: all passed');

// Boxes never overlap the next column, and links leaving one side of a box spread down it
{
	const W = G.layout(graph, 1650);
	const xs = [...new Set(Object.values(W.nodes).map(n => n.x))].sort((a, b) => a - b);
	for (let i = 1; i < xs.length; i++) same(xs[i] - xs[i - 1] >= W.nodeWidth + 40, true, 'room between columns ' + i);
	const starts = Object.keys(W.paths).filter(id => id.includes('master')).map(id => W.paths[id].fwd);
	same(new Set(starts.map(d => d.split(' ')[0])).size > 1 || starts.length < 2, true, 'links at master do not all meet at one point');
}

// An idle player (no packets in the window) still has a link from Game clients to their world
{
	const idle = G.build({ servers: { w: { key: 'w', type: 'WORLD', zone: 1200, instance: 1, label: 'World 1200', link: { connections: 2 },
		split: { clients: {}, master: {}, servers: {} }, packets_in: 0, packets_out: 0, bytes_in: 0, bytes_out: 0 } } }, {});
	same(idle.edges.some(e => e.from === 'clients'), true, 'idle player keeps the Game clients link');
}
