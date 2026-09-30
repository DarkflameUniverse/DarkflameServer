// Game text is never written into the dashboard: zone, item and character names and the game's currency names come
// from the client's locale in the viewer's language (routes/GameText.h; `game` and phrase()/zone_name() in templates,
// GameText.* in static/js/common.js). This fails when one of the game's names is written into a template, a script or a
// string in the routes' C++.
//
// Run by ctest: node game-text.test.mjs <dDashboardServer dir> <allowlist.json> [locale.xml]
// With a client's locale.xml, every zone name in it is looked for too.
//
// A legitimate use (not game text shown to people) goes in game-text-allowlist.json as {file, text, why}, or gets a
// "game-text: ok" comment on its line.
import { readFileSync, readdirSync, existsSync } from 'node:fs';
import { join, relative } from 'node:path';

const [root, allowPath, localePath] = process.argv.slice(2);

// Names the game uses, as its en_US locale has them (ZoneTable_*_DisplayDescription, Objects_*_name, mission senders)
const NAMES = new Set([
	'Venture Explorer', 'Return to the Venture Explorer', 'Avant Gardens', 'Avant Gardens Survival', 'Block Yard', 'Avant Grove',
	'Nimbus Station', 'Pet Cove', 'Vertigo Loop Racetrack', 'Battle of Nimbus Station', 'Nimbus Rock', 'Nimbus Isle',
	'Gnarled Forest', 'Cannon Cove Shooting Gallery', 'Keelhaul Canyon Racetrack', 'Chantey Shanty', 'Forbidden Valley',
	'Forbidden Valley Dragon Battle', 'Dragonmaw Chasm Racetrack', 'Raven Bluff', 'Starbase 3001', 'Deep Freeze',
	'Robot City', 'Moonbase', 'Portabello', 'Port-a-bello', 'LEGO Club', 'Crux Prime', 'Nexus Tower', 'Ninjago Monastery',
	'Battle Against Frakjaw', 'Spider Queen Battle',
	'Thinking Hat', 'Nexus Jawbox', 'Rocket Nose Cone', 'Duke Exeter', 'Hael Storm', 'Vanda Darkflame', 'Numb Chuck',
	'Beck Strongheart',
]);

// Every ZoneTable_<id>_DisplayDescription of a real locale.xml (en_US)
if (localePath && existsSync(localePath)) {
	const xml = readFileSync(localePath, 'utf8');
	const phrase = /<phrase id="ZoneTable_\d+_DisplayDescription">([\s\S]*?)<\/phrase>/g;
	for (const [, body] of xml.matchAll(phrase)) {
		const en = /<translation locale="en_US">([^<]+)<\/translation>/.exec(body);
		if (en && en[1].trim().length >= 6) NAMES.add(en[1].trim());
	}
}

// The game's currency and stat names written as labels (GameText.term / game.terms instead)
const TERMS = [
	/\bU-[Ss]core\b/,
	/\bUniverse Score\b/,
	/(>|'|")(Coins|Reputation|Imagination)(<|'|")/,
];

const escape = (s) => s.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
const NAME_PATTERNS = [...NAMES].map((name) => ({ name, re: new RegExp('(?<![\\w-])' + escape(name) + '(?![\\w-])') }));

const allow = JSON.parse(readFileSync(allowPath, 'utf8'));
const allowed = (file, text) => allow.some((a) => a.file === file && a.text === text);
const used = new Set();

// The text of a line people could see: comments dropped
function visible(line, kind) {
	const trimmed = line.trim();
	if (kind !== 'jinja2' && (trimmed.startsWith('//') || trimmed.startsWith('*') || trimmed.startsWith('/*'))) return '';
	let out = line.replace(/<!--.*?-->/g, '').replace(/\{#.*?#\}/g, '');
	// A trailing // comment (not in a URL)
	out = out.replace(/(^|[^:'"\\])\/\/.*$/, '$1');
	if (kind === 'cpp') {
		// Only the string literals of C++
		return [...out.matchAll(/"((?:[^"\\]|\\.)*)"/g)].map((m) => m[1]).join(' | ');
	}
	return out;
}

function* files(dir, exts) {
	for (const entry of readdirSync(dir, { withFileTypes: true })) {
		const path = join(dir, entry.name);
		if (entry.isDirectory()) yield* files(path, exts);
		else if (exts.some((e) => entry.name.endsWith(e)) && !entry.name.includes('.min.')) yield path;
	}
}

// What a line is flagged for
function found(line, kind) {
	if (line.includes('game-text: ok')) return [];
	const text = visible(line, kind);
	if (!text) return [];
	const names = [];
	for (const { name, re } of NAME_PATTERNS) if (re.test(text)) names.push(name);
	if (kind !== 'cpp') for (const re of TERMS) { const m = re.exec(text); if (m) names.push(m[0].replace(/^[>'"]|[<'"]$/g, '')); }
	return names;
}

let failures = 0;

// The scanner itself: what it must catch and what it must leave
const SELF = [
	['<p>Each world starts with its rent (Block Yard is free)</p>', 'jinja2', ['Block Yard']],
	["var labels = { title: 'U-score' };", 'js', ['U-score']],
	['<th>Coins</th>', 'jinja2', ['Coins']],
	['{ 4, "Opens LEGO Club" },', 'cpp', ['LEGO Club']],
	['// Checked in Avant Gardens', 'js', []],
	['	// Checked in Avant Gardens', 'cpp', []],
	['foo(); // like Nimbus Station', 'js', []],
	['<th>{{ game.terms.coins }}</th>', 'jinja2', []],
	['Where coins come from', 'jinja2', []],
	["'Nimbus Station' // game-text: ok", 'js', []],
];
for (const [line, kind, expected] of SELF) {
	const got = found(line, kind);
	if (JSON.stringify(got) !== JSON.stringify(expected)) {
		failures++;
		console.error(`scanner: ${JSON.stringify(line)} gave ${JSON.stringify(got)}, not ${JSON.stringify(expected)}`);
	}
}

const scanned = [
	...files(join(root, 'templates'), ['.jinja2']),
	...files(join(root, 'static', 'js'), ['.js']),
	...files(join(root, 'routes'), ['.cpp', '.h']),
];
for (const path of scanned) {
	const file = relative(root, path).replace(/\\/g, '/');
	const kind = path.endsWith('.jinja2') ? 'jinja2' : path.endsWith('.js') ? 'js' : 'cpp';
	readFileSync(path, 'utf8').split('\n').forEach((line, i) => {
		for (const name of found(line, kind)) {
			if (allowed(file, name)) { used.add(file + '\u0000' + name); continue; }
			failures++;
			console.error(`${file}:${i + 1}: "${name}" is game text written into the dashboard; look it up from the locale ` +
				`(GameText.h / game.terms / GameText.* in common.js), or allowlist it in game-text-allowlist.json`);
		}
	});
}

// Allowlist entries nothing needs any more
for (const a of allow) {
	if (!used.has(a.file + '\u0000' + a.text)) {
		failures++;
		console.error(`game-text-allowlist.json: ${a.file} "${a.text}" is not used any more; remove it`);
	}
}

if (scanned.length < 20) {
	failures++;
	console.error(`Only ${scanned.length} files found under ${root}`);
}

if (failures) {
	console.error(`${failures} problem(s)`);
	process.exit(1);
}
console.log(`game text: ${scanned.length} files clean of ${NAMES.size} game names`);
