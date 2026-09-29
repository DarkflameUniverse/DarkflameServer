// The sidebar's saved group state (the <script data-sidebar-state> in templates/header.jinja2), run on a small fake DOM.
// Run by ctest: node sidebar-state.test.mjs <header.jinja2>
import { readFileSync } from 'node:fs';
import vm from 'node:vm';

const [headerPath] = process.argv.slice(2);
const source = readFileSync(headerPath, 'utf8').match(/<script data-sidebar-state>([\s\S]*?)<\/script>/)[1];
let failures = 0;
const same = (actual, expected, what) => {
	if (JSON.stringify(actual) !== JSON.stringify(expected)) {
		failures++;
		console.error(`${what}: ${JSON.stringify(actual)} is not ${JSON.stringify(expected)}`);
	}
};

class ClassList {
	constructor(names) { this.set = new Set(names); }
	contains(n) { return this.set.has(n); }
	toggle(n, on) { if (on === undefined) on = !this.set.has(n); if (on) this.set.add(n); else this.set.delete(n); return on; }
	add(n) { this.set.add(n); }
}

// A sidebar with three groups; "nav-logs" holds the current page
function page(storage, shownByServer) {
	const groups = ['nav-inbox', 'nav-world', 'nav-logs'].map((id) => ({
		id,
		classList: new ClassList(shownByServer.includes(id) ? ['collapse', 'show'] : ['collapse']),
		querySelector: (sel) => (sel === '.active' && id === 'nav-logs' ? {} : null),
		closest: (sel) => (sel === '#sidebar' ? {} : null)
	}));
	const toggles = Object.fromEntries(groups.map((g) => [g.id, { classList: new ClassList([]), attrs: {}, setAttribute(k, v) { this.attrs[k] = v; } }]));
	const listeners = {};
	const html = { classList: new ClassList([]) };
	const document = {
		documentElement: html,
		querySelectorAll(sel) {
			if (sel === '#sidebar .nav-group > .collapse[id]') return groups;
			const m = sel.match(/^\[data-bs-target="#(.+)"\]$/);
			return m ? [toggles[m[1]]] : [];
		},
		addEventListener(type, fn) { (listeners[type] = listeners[type] || []).push(fn); }
	};
	const localStorage = {
		getItem: (k) => (k in storage ? storage[k] : null),
		setItem: (k, v) => { storage[k] = String(v); }
	};
	const window = {};
	vm.runInNewContext(source, { window, document, localStorage, JSON });
	const open = () => Object.fromEntries(groups.map((g) => [g.id, g.classList.contains('show')]));
	const fire = (type, id) => (listeners[type] || []).forEach((fn) => fn({ type, target: groups.find((g) => g.id === id) }));
	return { window, open, fire, toggles, html, listeners };
}

// Nothing saved: the server's choice (the current page's group) stays, and is kept for the next page
{
	const storage = {};
	const p = page(storage, ['nav-logs']);
	same(p.open(), { 'nav-inbox': false, 'nav-world': false, 'nav-logs': true }, 'nothing saved');
	same(JSON.parse(storage['dash.sidebar']).groups, { 'nav-inbox': false, 'nav-world': false, 'nav-logs': true }, 'kept');
}

// Opening and closing groups is saved, and the next page starts that way
{
	const storage = {};
	const p = page(storage, ['nav-logs']);
	p.fire('shown.bs.collapse', 'nav-world');
	p.fire('hidden.bs.collapse', 'nav-inbox');
	same(JSON.parse(storage['dash.sidebar']).groups, { 'nav-inbox': false, 'nav-world': true, 'nav-logs': true }, 'saved groups');
	const next = page(storage, ['nav-logs']);
	same(next.open(), { 'nav-inbox': false, 'nav-world': true, 'nav-logs': true }, 'next page');
	same(next.toggles['nav-world'].attrs['aria-expanded'], 'true', 'toggle follows the group');
	same(next.toggles['nav-inbox'].classList.contains('collapsed'), true, 'closed toggle marked collapsed');
}

// A group left open on another page stays open here
{
	const p = page({ 'dash.sidebar': JSON.stringify({ groups: { 'nav-inbox': true, 'nav-world': false } }) }, ['nav-logs']);
	same(p.open(), { 'nav-inbox': true, 'nav-world': false, 'nav-logs': true }, 'left open');
}

// The current page's group opens even when it was closed last time
{
	const p = page({ 'dash.sidebar': JSON.stringify({ groups: { 'nav-logs': false, 'nav-inbox': true } }) }, []);
	same(p.open(), { 'nav-inbox': true, 'nav-world': false, 'nav-logs': true }, 'current group opens');
}

// Broken or blocked storage: the server's choice
{
	const p = page({ 'dash.sidebar': '{nope' }, ['nav-inbox']);
	same(p.open(), { 'nav-inbox': true, 'nav-world': false, 'nav-logs': true }, 'broken storage');
}

// Hiding the menu is saved with the groups
{
	const storage = { 'dash.sidebar': JSON.stringify({ groups: { 'nav-world': true } }) };
	const p = page(storage, []);
	p.window.Sidebar.setHidden(true);
	same(JSON.parse(storage['dash.sidebar']), { groups: { 'nav-world': true, 'nav-inbox': false, 'nav-logs': true }, hidden: true }, 'hidden saved');
	same(p.html.classList.contains('sidebar-hidden'), true, 'hidden now');
	p.window.Sidebar.setHidden(false);
	same(p.html.classList.contains('sidebar-hidden'), false, 'shown again');
}

if (failures) {
	console.error(`${failures} failure(s)`);
	process.exit(1);
}
console.log('sidebar state: all passed');
