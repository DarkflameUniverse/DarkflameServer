/**
 * The Vanity page, Preview tab: what the world servers would load with the scheduled events that are on at some time,
 * or with events picked here. The server loads it as the worlds do (VanityEvents::LoadWorld): which vanity files are
 * read and why (as the files say, or switched by an event), the events in the order they are laid on, the NPCs, the
 * conflicts between events and anything that couldn't be read. #preview:ID previews that event on its own.
 */
(function () {
	'use strict';

	var events = [];
	var pending = null; // an event id from the hash, previewed once the events are known
	function get(id) { return document.getElementById(id); }

	function toInput(ts) {
		var d = new Date(ts * 1000);
		d.setMinutes(d.getMinutes() - d.getTimezoneOffset());
		return d.toISOString().slice(0, 16);
	}

	function renderEvents() {
		var chosen = {};
		Array.prototype.forEach.call(document.querySelectorAll('[data-vp-event]:checked'), function (c) { chosen[c.value] = true; });
		if (pending) chosen[pending] = true;
		get('vpEvents').innerHTML = events.map(function (e) {
			return '<div class="form-check"><input class="form-check-input" type="checkbox" data-vp-event value="' + e.id + '" id="vp' + e.id + '"' + (chosen[e.id] ? ' checked' : '') + '>' +
				'<label class="form-check-label" for="vp' + e.id + '">' + esc(e.name) + ' <span class="text-body-secondary">(priority ' + esc(e.priority) + (e.on ? ', on now' : '') + ')</span></label></div>';
		}).join('') || '<span class="text-body-secondary">No event changes the vanity NPCs yet.</span>';
	}

	// Why a file is or isn't read
	function why(f) {
		if (f.switchedBy && f.loaded) return f.includedBy ? 'switched on by <strong>' + esc(f.switchedBy) + '</strong> (off in ' + esc(f.includedBy) + ')' : 'switched on by <strong>' + esc(f.switchedBy) + '</strong>';
		if (f.switchedBy) return 'switched off by <strong>' + esc(f.switchedBy) + '</strong>' + (f.includedBy ? ' (' + (f.enabled ? 'on' : 'off') + ' in ' + esc(f.includedBy) + ')' : '');
		if (!f.includedBy) return f.loaded ? 'where the worlds start' : 'couldn\'t be read';
		if (!f.enabled) return 'off in ' + esc(f.includedBy);
		return f.loaded ? 'on in ' + esc(f.includedBy) : 'on in ' + esc(f.includedBy) + ', but couldn\'t be read';
	}

	function filesTable(d) {
		var base = {};
		d.baseFiles.forEach(function (f) { base[f.name] = f.loaded; });
		return '<div class="table-responsive"><table class="table table-sm align-middle mb-0"><thead><tr><th>Vanity file</th><th>Loaded</th><th>Why</th></tr></thead><tbody>' +
			d.files.map(function (f) {
				var changed = !!base[f.name] !== f.loaded;
				return '<tr' + (changed ? ' class="vanity-changed"' : '') + '><td><a class="font-monospace" href="#file:' + esc(f.name) + '" data-vp-file="' + esc(f.name) + '">' + esc(f.name) + '</a></td>' +
					'<td>' + (f.loaded ? '<span class="badge vanity-badge-loaded">loaded</span>' : '<span class="badge vanity-badge-off">not loaded</span>') +
					(changed ? ' <span class="small text-body-secondary">(' + (f.loaded ? 'not' : 'loaded') + ' without events)</span>' : '') + '</td>' +
					'<td class="small">' + why(f) + '</td></tr>';
			}).join('') + '</tbody></table></div>';
	}

	function eventLine(e) {
		var p = e.parts.map(function (part) {
			var bits = Object.keys(part.fileSwitches).map(function (f) { return f + (part.fileSwitches[f] ? ' on' : ' off'); });
			if (part.file) bits.push('overlay ' + part.file);
			if (part.removals.length) bits.push('takes out ' + part.removals.join(', '));
			return bits.join('; ');
		}).join(' / ');
		return '<li><a href="/events#event:' + e.id + '">' + esc(e.name) + '</a> <span class="text-body-secondary">(priority ' + esc(e.priority) + ')</span>' + (p ? ': ' + esc(p) : '') + '</li>';
	}

	function run(body) {
		var out = get('vpOut');
		out.innerHTML = '<p class="text-body-secondary small">Loading&hellip;</p>';
		api.post('/api/vanity/preview', body).then(function (d) {
			if (!d.success) { out.innerHTML = '<div class="alert alert-danger small">' + esc(d.error || 'Failed') + '</div>'; return; }
			var problems = d.conflicts.map(function (c) { return 'NPC <strong>' + esc(c.npc) + '</strong> is changed by ' + esc(c.events.join(', ')) + '; ' + esc(c.winner) + ' wins.'; })
				.concat(d.fileConflicts.map(function (c) {
					return 'File <strong>' + esc(c.file) + '</strong> is switched by ' + esc(c.switches.map(function (s) { return s.event + ' (' + (s.on ? 'on' : 'off') + ')'; }).join(', ')) +
						'; ' + esc(c.winner) + ' wins.';
				}));
			out.innerHTML = '<div class="card mb-3"><div class="card-body">' +
				'<p class="mb-2">' + ('at' in body || !body.ids ? 'At ' + esc(fmt.unix(d.at)) + ': ' : '') + (d.events.length ? d.events.length + ' event(s) with vanity changes, laid on in this order:' : 'no event with vanity changes.') + '</p>' +
				(d.events.length ? '<ol class="small mb-2">' + d.events.map(eventLine).join('') + '</ol>' : '') +
				'<p class="small mb-0"><strong>' + esc(d.npcs) + '</strong> NPCs and props: ' + esc(d.fileNpcs) + ' from the vanity files that are loaded, then the overlays and removals. ' +
				'Without any event: ' + esc(d.baseNpcs) + '.</p></div></div>' +
				(problems.length ? '<div class="alert alert-warning small">' + problems.join('<br>') + '</div>' : '') +
				(d.warnings.length ? '<div class="alert alert-secondary small">' + d.warnings.map(esc).join('<br>') + '</div>' : '') +
				'<div class="card mb-3"><div class="card-header"><h3 class="h6 mb-0">Vanity files</h3></div><div class="card-body">' + filesTable(d) +
				'<div class="small text-body-secondary mt-2">Highlighted: loaded or not because of an event.</div></div></div>' +
				'<details class="card"><summary class="card-header">The merged vanity XML</summary><div class="card-body">' +
				'<div class="d-flex justify-content-end mb-1"><button type="button" class="btn btn-sm btn-outline-secondary" id="vpCopy">Copy XML</button></div>' +
				'<pre class="vanity-xml border rounded p-2 mb-0"></pre></div></details>';
			out.querySelector('pre').textContent = d.xml;
			get('vpCopy').addEventListener('click', function () { navigator.clipboard.writeText(d.xml).then(function () { toast('Copied', 'success'); }); });
		});
	}

	function runFromForm() {
		if (get('vpChosen').checked) {
			return run({ ids: Array.prototype.map.call(document.querySelectorAll('[data-vp-event]:checked'), function (c) { return Number(c.value); }) });
		}
		var value = get('vpTime').value;
		run(value ? { at: Math.floor(new Date(value).getTime() / 1000) } : {});
	}

	document.addEventListener('vanity:data', function (e) {
		events = e.detail.events || [];
		renderEvents();
		if (pending) {
			get('vpChosen').checked = true;
			run({ ids: [Number(pending)] });
			pending = null;
		}
	});

	get('vpNow').addEventListener('click', function () { get('vpTime').value = toInput(Math.floor(Date.now() / 1000)); get('vpAt').checked = true; });
	get('vpTime').addEventListener('change', function () { get('vpAt').checked = true; });
	get('vpEvents').addEventListener('change', function () { get('vpChosen').checked = true; });
	get('vpRun').addEventListener('click', runFromForm);
	// A file in the table opens it on the other tab
	get('vpOut').addEventListener('click', function (e) {
		var link = e.target.closest('[data-vp-file]');
		if (!link) return;
		e.preventDefault();
		bootstrap.Tab.getOrCreateInstance(get('vanityTabFiles')).show();
		var item = document.querySelector('#vanityFiles [data-select-file="' + CSS.escape(link.dataset.vpFile) + '"]');
		if (item) item.click();
	});
	// The first time the tab is shown, preview now
	get('vanityTabPreview').addEventListener('shown.bs.tab', function () { if (!get('vpOut').querySelector('.card')) runFromForm(); });

	get('vpTime').value = toInput(Math.floor(Date.now() / 1000));
	var hash = decodeURIComponent(location.hash.slice(1));
	if (hash.indexOf('preview:') === 0) pending = hash.slice(8);
})();
