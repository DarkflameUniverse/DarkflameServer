// The Performance page's layouts (static/js/perf-charts.js): flame graph, slow frame timeline, stacked phases.
// Run by ctest: node perf-charts.test.mjs <perf-charts.js>
import { readFileSync } from 'node:fs';
import vm from 'node:vm';

const [scriptPath] = process.argv.slice(2);
const context = { window: {} };
vm.createContext(context);
vm.runInContext(readFileSync(scriptPath, 'utf8'), context);
const C = context.window.PerfCharts;
let failures = 0;
const same = (actual, expected, what) => {
	if (JSON.stringify(actual) !== JSON.stringify(expected)) {
		failures++;
		console.error(`${what}: ${JSON.stringify(actual)} is not ${JSON.stringify(expected)}`);
	}
};

// A session's tree as PerfHistory::ProfileJson sends it
const nodes = [
	{ label: 'All frames', depth: 0, total_us: 1000 },
	{ label: 'Entities', depth: 1, total_us: 600 },
	{ label: 'Script timer', depth: 2, total_us: 200 },
	{ label: 'Physics step', depth: 1, total_us: 300 },
	{ label: 'Tiny', depth: 1, total_us: 0.5 },
	{ label: 'Under tiny', depth: 2, total_us: 0.5 },
];
same(Array.from(C.parents(nodes)), [-1, 0, 1, 0, 0, 4], 'parents');

const whole = C.flameLayout(nodes, 0, 0.001).map((r) => [r.index, +r.x.toFixed(3), +r.w.toFixed(3), r.row]);
same(whole, [[0, 0, 1, 0], [1, 0, 0.6, 1], [2, 0, 0.2, 2], [3, 0.6, 0.3, 1]], 'flame graph of the whole tree, too narrow ones left out');

// Zoomed into Entities: its ancestors span the width, it and its children are scaled to it
const zoomed = C.flameLayout(nodes, 1, 0).map((r) => [r.index, +r.x.toFixed(3), +r.w.toFixed(3), r.row, !!r.ancestor]);
same(zoomed, [[0, 0, 1, 0, true], [1, 0, 1, 1, false], [2, 0, 0.333, 2, false]], 'flame graph zoomed in');

// A slow frame: bars from their first start
const scopes = [
	{ label: 'Frame', depth: 0, start_ms: 0, total_ms: 100 },
	{ label: 'LoadPlayer', depth: 1, start_ms: 10, total_ms: 80 },
	{ label: 'Late', depth: 1, start_ms: 95, total_ms: 20 },
];
same(C.timelineLayout(scopes, 100).map((r) => [r.index, +r.x.toFixed(2), +r.w.toFixed(2), r.row]), [[0, 0, 1, 0], [1, 0.1, 0.8, 1], [2, 0.95, 0.05, 1]], 'timeline');

// Phases: the ones with time in their fixed colours, scripts and log flushes folded into other
const series = C.phaseSeries({ packets: [1, 2], entities: [0, 0], physics: [3, null], scripts: [1, null], log_flush: [0.5, null], other: [1, null], cdclient: [0, 9] });
same(series.map((s) => [s.key, s.color, Array.from(s.values)]), [
	['packets', 'var(--tr-1)', [1, 2]],
	['physics', 'var(--tr-3)', [3, null]],
	['cdclient', 'var(--tr-6)', [0, 9]],
	['other', 'var(--tr-other)', [2.5, null]],
], 'phase series');

if (failures) {
	console.error(`${failures} failure(s)`);
	process.exit(1);
}
console.log('perf-charts: all passed');
