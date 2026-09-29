// Which links the dashboard follows without a reload, and the rules its in-place updates use (static/js/nav.js).
// Run by ctest: node nav.test.mjs <nav.js>
import { readFileSync } from 'node:fs';
import vm from 'node:vm';

const [navPath] = process.argv.slice(2);
const window = {};
vm.runInNewContext(readFileSync(navPath, 'utf8'), { window });
const R = window.NavRules;
let failures = 0;
const same = (actual, expected, what) => {
	if (JSON.stringify(actual) !== JSON.stringify(expected)) {
		failures++;
		console.error(`${what}: ${JSON.stringify(actual)} is not ${JSON.stringify(expected)}`);
	}
};

const here = new URL('http://127.0.0.1:2006/accounts?x=1');
const kind = (href) => R.kind(new URL(href, here), here);

// Dashboard pages are swapped in
same(kind('/characters'), 'swap', 'a list page');
same(kind('/characters/1234'), 'swap', 'a detail page');
same(kind('/properties/5/3d'), 'swap', 'the 3D view (the fetched page then asks for a normal load)');
same(kind('/system_log?server=world'), 'swap', 'a page with a query');
same(kind('/activity_log#search=12'), 'swap', 'another page with a hash filter');
same(kind('/accounts'), 'swap', 'the same page without its query');
same(kind('/accounts?x=1'), 'swap', 'the page that is open, again');
same(kind('/'), 'swap', 'home');
same(kind('/status'), 'swap', 'server status');

// Only the fragment of this page: the browser does it
same(kind('/accounts?x=1#tab'), 'hash', 'fragment of this page');
same(kind('#tab'), 'hash', 'bare fragment');

// Normal loads
same(kind('https://example.com/characters'), null, 'another site');
same(kind('http://127.0.0.1:2007/characters'), null, 'another port');
same(kind('mailto:a@b.c'), null, 'mailto');
for (const path of ['/api/backups/1/download', '/css/dashboard.css', '/js/nav.js', '/ws', '/metrics', '/login', '/logout',
	'/register', '/forgot_password', '/reset_password?token=1', '/verify_email', '/oauth2/callback', '/status/widget', '/ugc/model.nif', '/files/log.zip']) {
	same(kind(path), null, path);
}
same(kind('/logins'), 'swap', 'a path that only starts like a signed-out page');

// Pages of one kind share what was learnt about them
same(R.pageKind('/properties/12/3d'), '/properties/*/3d', 'a 3D view');
same(R.pageKind('/characters/1152921504606846978'), '/characters/*', 'a character');
same(R.pageKind('/world3d'), '/world3d', 'no numbers');

// Scripts: classic JavaScript runs again, data and modules don't
for (const type of [null, '', 'text/javascript', 'application/javascript', 'TEXT/JavaScript ', 'application/x-javascript', 'text/ecmascript']) same(R.runnable(type), true, 'runs: ' + type);
for (const type of ['application/json', 'module', 'importmap', 'text/template']) same(R.runnable(type), false, 'inert: ' + type);

// Child lists line up only when every key does
same(R.sameKeys(['DIV#a', '#3', 'P'], ['DIV#a', '#3', 'P']), true, 'same keys');
same(R.sameKeys(['DIV#a', 'P'], ['DIV#a', '#3', 'P']), false, 'one more');
same(R.sameKeys(['DIV#a'], ['DIV#b']), false, 'other id');
same(R.sameKeys([], []), true, 'empty');

// A server class change touches only the classes the server changed, not those scripts added
same(R.classChanges('badge text-bg-warning', 'badge text-bg-success'), { remove: ['text-bg-warning'], add: ['text-bg-success'] }, 'badge colour');
same(R.classChanges('', 'a  b'), { remove: [], add: ['a', 'b'] }, 'from nothing');
same(R.classChanges(null, null), { remove: [], add: [] }, 'nothing');

if (failures) {
	console.error(`${failures} failure(s)`);
	process.exit(1);
}
console.log('nav rules: all passed');
