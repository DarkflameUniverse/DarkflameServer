// Following a player across worlds (static/js/world3d-core.js): when the live view and capture playback switch the 3D
// scene, and the timeline's world change markers.
// Run by ctest: node world3d-follow.test.mjs <world3d-core.js>
import { pathToFileURL } from 'node:url';

const [modulePath] = process.argv.slice(2);
const W = await import(pathToFileURL(modulePath).href);
let failures = 0;
const same = (actual, expected, what) => {
	if (JSON.stringify(actual) !== JSON.stringify(expected)) {
		failures++;
		console.error(`${what}: ${JSON.stringify(actual)} is not ${JSON.stringify(expected)}`);
	}
};

// ---- live: one player_positions report lists every world's players ----
const at = (id, zone, instance, clone = 0) => ({ id, name: 'N' + id, zone, instance, clone, x: 0, y: 0, z: 0 });
same(W.followedMove('7', [at('7', 1100, 3)], 1100, 0), null, 'still in the zone shown');
same(W.followedMove('7', [at('7', 1100, 3)], 1100, 3), null, 'still in the instance shown');
same(W.followedMove('7', [at('7', 1100, 4)], 1100, 0), null, 'another instance while all instances are shown');
same(W.followedMove('7', [at('7', 1100, 4, 2)], 1100, 3), { zone: 1100, instance: 4, clone: 2 }, 'another instance of the picked one');
same(W.followedMove('7', [at('8', 1100, 3), at('7', 1200, 5)], 1100, 0), { zone: 1200, instance: 5, clone: 0 }, 'moved to another zone');
same(W.followedMove('7', [at('7', 1100, 3), at('7', 1200, 5)], 1100, 3), null, 'the old world still reports them: wait');
same(W.followedMove('7', [], 1100, 3), null, 'between worlds (nobody reports them): wait');
same(W.followedMove('7', [at('7', 0, 1)], 1100, 3), null, 'character select has nothing to draw');
same(W.followedMove(null, [at('7', 1200, 5)], 1100, 3), null, 'nobody followed');
same(W.followedMove('7', undefined, 1100, 3), null, 'no report');

// ---- capture playback: the positions route's worlds ----
const worlds = [
	{ character: 'a', name: 'Alpha', t: 0, zone: 0, zoneName: '', instance: 1, clone: 0 },
	{ character: 'a', name: 'Alpha', t: 5, zone: 1000, zoneName: 'First', instance: 2, clone: 0 },
	{ character: 'b', name: 'Beta', t: 6, zone: 1100, zoneName: 'Second', instance: 3, clone: 0 },
	{ character: 'a', name: 'Alpha', t: 20, zone: 1100, zoneName: 'Second', instance: 3, clone: 0 },
	{ character: 'a', name: 'Alpha', t: 30, zone: 0, zoneName: '', instance: 1, clone: 0 },
	{ character: 'a', name: 'Alpha', t: 40, zone: 1100, zoneName: 'Second', instance: 3, clone: 0 },
	{ character: 'a', name: 'Alpha', t: 50, zone: 1100, zoneName: 'Second', instance: 9, clone: 0 }
];
same(W.worldAt(worlds, 'a', 1), null, 'in character select before any world');
same(W.worldAt(worlds, 'a', 5).zone, 1000, 'reached the first world');
same(W.worldAt(worlds, 'a', 19.9).zone, 1000, 'still there just before the move');
same(W.worldAt(worlds, 'a', 20).zone, 1100, 'moved at the transfer');
same(W.worldAt(worlds, 'a', 35).t, 20, 'character select in between keeps the world before it');
same(W.worldAt(worlds, 'a', 55).instance, 9, 'another instance');
same(W.worldAt(worlds, 'b', 5), null, 'another character not there yet');

same(W.captureSwitch(worlds, 'a', 10, 1000), null, 'already showing their world');
same(W.captureSwitch(worlds, 'a', 25, 1000).zone, 1100, 'switch forward at the transfer');
same(W.captureSwitch(worlds, 'a', 10, 1100).zone, 1000, 'switch back when scrubbing back');
same(W.captureSwitch(worlds, 'a', 55, 1100), null, 'another instance of the zone shown: players of every instance are drawn');
same(W.captureSwitch(worlds, 'a', 1, 1100), null, 'before their first world the view stays');
same(W.captureSwitch(worlds, null, 25, 1000), null, 'nobody followed');

const markers = W.worldMarkers(worlds, 100);
same(markers.map((m) => [m.character, m.t, m.zone, m.instance]), [['a', 5, 1000, 2], ['a', 20, 1100, 3], ['a', 50, 1100, 9]],
	'changes: logging in from character select counts, the first world seen does not, back to the same world after character select does not');
same(markers[1].at, 0.2, 'placed along the timeline');
same(markers[1].zoneName, 'Second', 'named from the route (the locale)');
same(W.worldMarkers(worlds, 100, 'b'), [], 'only the followed character');
same(W.worldMarkers(worlds, 10).map((m) => m.at), [0.5, 1, 1], 'clamped to the timeline');
same(W.worldMarkers([], 10), [], 'nothing captured');

const esc = (s) => String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
const html = W.markersHtml([{ t: 20, at: 0.2, name: 'A<b>', zone: 1100, zoneName: 'Second', instance: 3 }], esc, true);
same(html.includes('left:20.000%'), true, 'marker position');
same(html.includes('data-t="20"'), true, 'seeks to its time');
same(html.includes('<span>A&lt;b&gt;: Second</span>'), true, 'labelled with the character and zone, escaped');
same(html.includes('title="A&lt;b&gt; → Second #3"'), true, 'title with the instance');
same(W.markersHtml([{ t: 1, at: 0, zone: 1200, zoneName: '', instance: 1 }], esc).includes('<span>Zone 1200</span>'), true, 'a zone without a name');

if (failures) {
	console.error(failures + ' failed');
	process.exit(1);
}
console.log('world3d follow: ok');
