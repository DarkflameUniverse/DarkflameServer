/**
 * The UGC server's settings on the UGC page, beside what they change: any element with data-ugc-settings="<section>,
 * <section>" shows those sections of the settings catalog (the same entries, values and save path as the Settings
 * page, /api/settings, so a change in either place is the other's too; the servers reload at once). Settings only read
 * at start say so. Each has a link to it on the Settings page. Only for those with the settings permission.
 */
(function () {
	'use strict';

	if (!window.DASH || !DASH.can('settings')) return;
	var boxes = document.querySelectorAll('[data-ugc-settings]');
	if (!boxes.length) return;
	var settings = null;

	function key(s) { return s.file + '/' + s.name; }
	function sourceText(s) {
		if (s.source === 'web') return 'set here';
		if (s.source === 'file') return 'from ' + s.file;
		if (s.source === 'env') return 'from the environment';
		return 'default';
	}
	function input(s) {
		var id = 'ugcset-' + key(s).replace(/[^a-z0-9_]/gi, '-'), value = s.value === undefined || s.value === null ? '' : String(s.value);
		var attrs = ' id="' + id + '" data-ugc-setting="' + esc(key(s)) + '"' + (s.fileOnly ? ' disabled' : '');
		if (s.type === 'bool') {
			return '<div class="form-check form-switch mb-0"><input class="form-check-input" type="checkbox"' + attrs + (value === '1' || value === 'true' ? ' checked' : '') + '></div>';
		}
		if (s.type === 'choice') {
			return '<select class="form-select form-select-sm"' + attrs + '>' + (s.choices || []).map(function (c, i) {
				return '<option value="' + esc(c) + '"' + (c === value ? ' selected' : '') + '>' + esc((s.choiceLabels || [])[i] || c) + '</option>';
			}).join('') + '</select>';
		}
		var number = s.type === 'int' || s.type === 'float';
		return '<input class="form-control form-control-sm" type="' + (number ? 'number' : 'text') + '"' + attrs + ' value="' + esc(value) + '"' +
			(number && s.min !== undefined ? ' min="' + s.min + '"' : '') + (number && s.max !== undefined ? ' max="' + s.max + '"' : '') +
			(s.type === 'float' ? ' step="any"' : '') + '>';
	}
	function row(s) {
		return '<div class="col-md-6 col-xl-4"><div class="border rounded p-2 h-100"><div class="d-flex justify-content-between gap-2"><label class="fw-semibold" for="ugcset-' +
			esc(key(s).replace(/[^a-z0-9_]/gi, '-')) + '">' + esc(s.title) + '</label><a class="text-nowrap" href="/settings#' + encodeURIComponent(key(s)) + '" title="The same setting on the Settings page">Open in Settings</a></div>' +
			'<div class="d-flex align-items-center gap-2 mt-1">' + input(s) + (s.unit ? '<span class="text-body-secondary">' + esc(s.unit) + '</span>' : '') + '</div>' +
			'<div class="text-body-secondary mt-1">' + (s.description ? esc(s.description) + ' ' : '') + 'Default ' + esc(s.default === '' ? '(empty)' : s.default) + '; ' +
			'<span data-ugc-source="' + esc(key(s)) + '">' + esc(sourceText(s)) + '</span>.' + (s.restart ? ' ' + fmt.badge('restart', 'warning') : '') + '</div></div></div>';
	}
	function render(box) {
		var sections = box.dataset.ugcSettings.split(',').map(function (x) { return x.trim(); });
		var shown = settings.filter(function (s) { return sections.indexOf(s.section) !== -1 && !s.secret; });
		var open = box.querySelector('details[open]') ? ' open' : '';
		box.innerHTML = '<details class="mt-2"' + open + '><summary class="small fw-semibold">' + esc(box.dataset.title || 'Settings') + ' (' + shown.length + ')</summary>' +
			'<div class="small mt-2"><div class="text-body-secondary mb-2">The same settings as on the <a href="/settings#ugc">Settings page</a>; a change saves at once and the servers reload. ' +
			'Those marked restart are only read when the UGC server starts.</div><div class="row g-2">' + shown.map(row).join('') + '</div></div></details>';
	}
	function load() {
		return api.get('/api/settings').then(function (d) {
			if (!d.success) return;
			settings = d.settings;
			boxes.forEach(render);
		});
	}

	document.addEventListener('change', function (e) {
		var el = e.target.closest && e.target.closest('[data-ugc-setting]');
		if (!el || !settings) return;
		var s = settings.find(function (x) { return key(x) === el.dataset.ugcSetting; });
		if (!s) return;
		var value = s.type === 'bool' ? el.checked : el.value;
		// A value the .ini file (or environment) sets would win over one set here unless this one is told to win
		api.post('/api/settings', { file: s.file, name: s.name, value: value, webWins: s.source === 'file' || s.source === 'env' || s.webWins }).then(function (d) {
			toast(d.success ? d.message : (d.error || 'Not saved'), d.success ? 'success' : 'danger');
			if (!d.success) return;
			s.value = d.value;
			s.source = 'web';
			document.querySelectorAll('[data-ugc-source="' + CSS.escape(key(s)) + '"]').forEach(function (x) { x.textContent = sourceText(s); });
		});
	});
	if (window.Live) Live.on('settings', load);
	load();
})();
