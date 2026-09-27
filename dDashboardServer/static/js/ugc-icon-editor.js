/**
 * The /ugc page's icon editor: one panel for everything an icon's look can be set to, built from the UGC server's one
 * list of parameters (/api/ugc/icon/params): the 3D pose view (ugc-pose.js: camera, the model's turn, the sun, the
 * shift and border, dragged or wheeled), the sliders of every parameter (camera, framing, model, sun, light, look) kept
 * in step with it both ways, and a live preview drawn by the UGC server as things change. The values can be saved as
 * the preset of the item's type or as the item's (or module combination's) own, and every icon of a type drawn again.
 */
(function () {
	'use strict';

	var GROUPS = [['camera', 'Camera'], ['framing', 'Border and shift'], ['model', 'Model turn'], ['sun', 'Sun'], ['light', 'Light'], ['look', 'Look']];
	var PARAMS = [], KINDS = [], paramsLoaded = null;
	var target = null;     // {model, id, modules, label}
	var layers = null;     // {kind, settings, preset, own}
	var values = {};
	var editor = null, editorLoading = null, previewTimer = null, previewSequence = 0, previewUrl = null;
	var canManage = window.DASH && DASH.can ? DASH.can('ugc_manage') : false;

	function $(id) { return document.getElementById(id); }
	function digits(step) { return step >= 1 ? 0 : step >= 0.1 ? 1 : 2; }
	function shown(p, value) { return (+value).toFixed(digits(p.step)) + (p.unit === 'degrees' ? '°' : ''); }

	function loadParams() {
		if (!paramsLoaded) {
			paramsLoaded = api.get('/api/ugc/icon/params').then(function (d) {
				if (!d.success) throw new Error(d.error || 'No parameters');
				PARAMS = d.params;
				KINDS = d.kinds || [];
				buildControls();
				return d;
			});
		}
		return paramsLoaded;
	}

	function buildControls() {
		var known = GROUPS.map(function (g) { return g[0]; });
		var groups = GROUPS.concat(PARAMS.filter(function (p) { return known.indexOf(p.group) === -1; })
			.map(function (p) { return [p.group, p.group]; }).filter(function (g, i, all) { return all.findIndex(function (x) { return x[0] === g[0]; }) === i; }));
		$('framingControls').innerHTML = groups.map(function (g) {
			var list = PARAMS.filter(function (p) { return p.group === g[0]; });
			if (!list.length) return '';
			return '<div class="col-md-6 col-xl-4"><fieldset class="border rounded px-2 pb-1 h-100"><legend class="float-none w-auto px-1 fs-6 mb-0">' + esc(g[1]) + '</legend>' +
				list.map(function (p) {
					return '<label class="d-flex justify-content-between mb-0" for="icon_' + esc(p.key) + '" title="' + esc(p.description || '') + '"><span>' + esc(p.label) +
						'</span><span id="icon_' + esc(p.key) + '_value" class="font-monospace"></span></label><input type="range" class="form-range" id="icon_' + esc(p.key) +
						'" data-key="' + esc(p.key) + '" min="' + p.min + '" max="' + p.max + '" step="' + p.step + '">';
				}).join('') + '</fieldset></div>';
		}).join('');
	}

	// Sliders from `values` (only the keys given)
	function showValues(keys) {
		(keys || PARAMS.map(function (p) { return p.key; })).forEach(function (key) {
			var p = PARAMS.find(function (x) { return x.key === key; });
			if (!p || values[key] === undefined) return;
			$('icon_' + key).value = values[key];
			$('icon_' + key + '_value').textContent = shown(p, values[key]);
		});
	}

	function layered(which) {
		var out = {};
		['settings', 'preset', 'own'].slice(0, which).forEach(function (layer) {
			var v = layers && layers[layer];
			if (v) for (var k in v) out[k] = v[k];
		});
		return out;
	}

	function setAll(next, why) {
		values = next;
		showValues();
		if (editor) editor.setValues(values);
		if (why) $('framingState').textContent = why;
		schedulePreview(0);
	}

	function itemQuery() {
		return target.model ? 'kind=model&id=' + encodeURIComponent(target.id) : 'kind=modular&modules=' + encodeURIComponent(target.modules || '');
	}
	function itemBody(extra) {
		var body = target.model ? { kind: 'model', id: target.id } : { kind: 'modular', modules: target.modules };
		for (var k in extra) body[k] = extra[k];
		return body;
	}
	function kindLabel() {
		var k = KINDS.find(function (x) { return x.kind === (layers && layers.kind); });
		return k ? k.label : (layers ? layers.kind : '');
	}

	function loadLayers() {
		return api.get('/api/ugc/icon/settings?' + itemQuery()).then(function (d) {
			if (!d.success) throw new Error(d.error || 'No settings');
			layers = { kind: d.kind, settings: d.settings, preset: d.preset, own: d.own };
			$('framingKind').textContent = kindLabel();
			setAll(layered(3), d.own ? 'This item has its own values.' : d.preset ? 'The type’s preset.' : 'The default settings.');
		});
	}

	function ensureEditor() {
		if (editor) return Promise.resolve(editor);
		if (!editorLoading) {
			editorLoading = import('/js/ugc-pose.js').then(function (module) {
				var limits = {};
				PARAMS.forEach(function (p) { limits[p.key] = { min: p.min, max: p.max }; });
				editor = module.createPoseEditor($('poseView'), {
					limits: limits,
					onChange: function (changed) {
						for (var k in changed) values[k] = changed[k];
						showValues(Object.keys(changed));
						$('framingState').textContent = 'Changed, not saved.';
						schedulePreview(300);
					}
				});
				editor.setSunArrow($('sunArrowSwitch').checked);
				return editor;
			});
		}
		return editorLoading;
	}

	function loadMesh() {
		var url = target.model ? '/api/ugc/mesh/' + encodeURIComponent(target.id) + '?lod=0' : '/api/ugc/assembly?modules=' + encodeURIComponent(target.modules || '');
		$('poseStats').textContent = 'Loading the ' + (target.model ? 'model' : 'assembled modules') + '…';
		var wanted = target;
		return ensureEditor().then(function (e) {
			e.setValues(values);
			return e.load(url);
		}).then(function (r) {
			if (target !== wanted) return;
			$('poseStats').textContent = r.triangles.toLocaleString() + ' triangles' + (target.model ? ' (the made .nif, most detailed level)' : ' (the modules put together as the icon renderer does)') + '.';
		}).catch(function (e) {
			$('poseStats').textContent = 'The 3D view could not load: ' + e.message;
		});
	}

	function schedulePreview(delay) {
		clearTimeout(previewTimer);
		previewTimer = setTimeout(renderPreview, delay);
	}

	function renderPreview() {
		if (!target) return;
		var img = $('framingPreview'), sequence = ++previewSequence;
		img.style.opacity = 0.5;
		fetch('/api/ugc/icon/preview', { method: 'POST', credentials: 'same-origin', headers: { 'Content-Type': 'application/json', 'X-Requested-With': 'dashboard' },
			body: JSON.stringify(itemBody({ values: values })) }).then(function (r) {
			if (!r.ok) return r.json().then(function (d) { throw new Error(d.error || 'HTTP ' + r.status); });
			return r.blob();
		}).then(function (blob) {
			if (sequence !== previewSequence) return; // a newer one is on its way
			if (previewUrl) URL.revokeObjectURL(previewUrl);
			previewUrl = URL.createObjectURL(blob);
			img.src = previewUrl;
			img.style.opacity = 1;
		}).catch(function (e) {
			if (sequence !== previewSequence) return;
			img.style.opacity = 1;
			$('framingState').textContent = 'No preview: ' + e.message;
		});
	}

	function saved(d) { toast(d.success ? d.message : (d.error || 'Failed'), d.success ? 'success' : 'danger'); }

	// ---- the controls ----

	$('framingControls').addEventListener('input', function (e) {
		var key = e.target.dataset && e.target.dataset.key;
		if (!key) return;
		values[key] = parseFloat(e.target.value);
		showValues([key]);
		if (editor) editor.setValues(values);
		$('framingState').textContent = 'Changed, not saved.';
		schedulePreview(300);
	});
	$('poseModes').addEventListener('click', function (e) {
		var button = e.target.closest('[data-mode]');
		if (!button) return;
		this.querySelectorAll('[data-mode]').forEach(function (b) { b.classList.toggle('active', b === button); });
		if (editor) editor.setMode(button.dataset.mode);
	});
	$('sunArrowSwitch').addEventListener('change', function () { if (editor) editor.setSunArrow(this.checked); });
	$('framingPreviewButton').addEventListener('click', renderPreview);
	$('framingResetType').addEventListener('click', function () { setAll(layered(2), 'The type’s preset (not saved for this item).'); });
	$('framingResetDefaults').addEventListener('click', function () { setAll(layered(1), 'The default settings (not saved).'); });
	$('framingReset').addEventListener('click', function () {
		if (!target || !confirm('Remove this item’s own values and go back to its type’s?')) return;
		api.post('/api/ugc/icon/save', itemBody({ scope: 'item', values: null })).then(function (d) { saved(d); loadLayers(); });
	});
	$('framingSaveType').addEventListener('click', function () {
		if (!layers || !confirm('Use these values for every icon of the type "' + kindLabel() + '"? Icons already made keep theirs until they are drawn again.')) return;
		api.post('/api/ugc/icon/save', { scope: 'kind', kind: layers.kind, values: values }).then(function (d) {
			saved(d);
			if (d.success) layers.preset = Object.assign({}, values);
		});
	});
	$('framingSaveCombo').addEventListener('click', function () {
		if (!target) return;
		api.post('/api/ugc/icon/save', itemBody({ scope: 'item', values: values })).then(function (d) {
			saved(d);
			if (d.success) {
				layers.own = Object.assign({}, values);
				$('framingState').textContent = 'This item has its own values.';
			}
		});
	});
	$('framingRegenerate').addEventListener('click', function () {
		if (!layers || !confirm('Draw every stored icon of the type "' + kindLabel() + '" again with the saved values? (Only icons are drawn.)')) return;
		api.post('/api/ugc/icon/regenerate', { kind: layers.kind }).then(function (d) {
			toast(d.success ? d.queued + ' icon' + (d.queued === 1 ? '' : 's') + ' queued' : (d.error || 'Failed'), d.success ? 'success' : 'danger');
		});
	});
	document.querySelectorAll('#framingCard .ugc-manage').forEach(function (b) { b.classList.toggle('d-none', !canManage); });

	window.UgcIconEditor = {
		/** Opens the editor on an item: {model: true, id} or {model: false, modules, label} */
		open: function (item) {
			target = item;
			$('framingCard').classList.remove('d-none');
			$('framingSaveCombo').textContent = item.model ? 'Save for this model' : 'Save for this combination of modules';
			return loadParams().then(loadLayers).then(loadMesh).catch(function (e) { $('framingState').textContent = e.message; });
		},
		close: function () {
			target = null;
			clearTimeout(previewTimer);
		},
		/** The kinds of icon (player models, each car or rocket build type) with a sample each to edit on */
		kinds: function () { return loadParams().then(function () { return KINDS; }); },
		editor: function () { return editor; },
		values: function () { return Object.assign({}, values); }
	};
})();
