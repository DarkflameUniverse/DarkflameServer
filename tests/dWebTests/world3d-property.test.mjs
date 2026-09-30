// A property in a capture replay (static/js/world3d-core.js): which models stand where at the playhead, the property
// data then, which property world the zone on screen shows, and the behavior ticks.
// Run by ctest: node world3d-property.test.mjs <world3d-core.js>
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

// The property route's shape: one model there from the start that moves at 5s, one placed at 4s and picked up at 8s
const world = {
	zone: 1150, instance: 7, clone: 42,
	info: [{ t: 1, name: 'Old name', reputation: 10 }, { t: 9, name: 'New name', reputation: 25 }],
	counts: [{ t: 1, count: 1 }, { t: 4, count: 2 }, { t: 8, count: 1 }],
	models: [
		{ object: '1', lot: 14, spans: [{ from: 2, to: 5, position: [1, 2, 3] }, { from: 5, to: null, position: [5, 2, 5] }] },
		{ object: '2', lot: 14, spans: [{ from: 4, to: 8, position: [10, 2, -4] }] }
	],
	events: [
		{ kind: 'placed', t: 3, object: '2' },
		{ kind: 'behavior', t: 6, message: 'PLAY_BEHAVIOR_SOUND', object: '1', lot: 14 },
		{ kind: 'removed', t: 8, object: '2', reason: 'picked up' }
	]
};
const at = (t) => {
	const p = W.propertyAt(world, t);
	return { models: p.models.map((m) => m.model.object + '@' + m.span.position.join(',')), name: p.info && p.info.name, count: p.count && p.count.count };
};

same(at(0), { models: [], name: null, count: null }, 'before anything was captured');
same(at(2), { models: ['1@1,2,3'], name: 'Old name', count: 1 }, 'the model already there');
same(at(4.5), { models: ['1@1,2,3', '2@10,2,-4'], name: 'Old name', count: 2 }, 'the placed model appears');
same(at(5), { models: ['1@5,2,5', '2@10,2,-4'], name: 'Old name', count: 2 }, 'the first one moved (a span ends where the next begins)');
same(at(8), { models: ['1@5,2,5'], name: 'Old name', count: 1 }, 'the second one was picked up');
same(at(20), { models: ['1@5,2,5'], name: 'New name', count: 1 }, 'still there at the end, renamed');
// Seeking back gives the same as playing forward
same(at(4.5), { models: ['1@1,2,3', '2@10,2,-4'], name: 'Old name', count: 2 }, 'seeking back');
same(W.propertyAt(null, 3), { models: [], info: null, count: null }, 'no property');

// Which world: the followed character's, else the first of the zone
const other = { ...world, instance: 9, clone: 43 };
const moves = [{ character: 'c', t: 0, zone: 1150, instance: 9, clone: 43 }, { character: 'c', t: 10, zone: 1150, instance: 7, clone: 42 }];
same(W.propertyWorldFor([world, other], 1150, moves, 'c', 5).clone, 43, 'the followed character\'s property');
same(W.propertyWorldFor([world, other], 1150, moves, 'c', 11).clone, 42, 'they moved to another property');
same(W.propertyWorldFor([world, other], 1150, moves, null, 5).clone, 42, 'nobody followed: the first');
same(W.propertyWorldFor([world, other], 1100, moves, 'c', 5), null, 'not a property zone');

// Behavior ticks
same(W.behaviorMarkers(world, 12), [{ t: 6, at: 0.5, title: 'PLAY_BEHAVIOR_SOUND (LOT 14)' }], 'one behavior tick');
const html = W.behaviorMarkersHtml(W.behaviorMarkers(world, 12), (s) => String(s).replace(/</g, '&lt;'));
same(html.includes('data-t="6"') && html.includes('left:50.000%'), true, 'the tick seeks to its message');

if (failures) {
	console.error(`${failures} failure(s)`);
	process.exit(1);
}
console.log('world3d property: ok');
