/**
 * The Performance page's drawing: the frame time and phase charts, the flame graph of a profiling session and the
 * timeline of a slow frame. The layouts are plain functions (tested with node: tests/dWebTests/perf-charts.test.mjs);
 * the draw functions put SVG into the page.
 *
 * Scope trees come in pre-order with a depth, as the server sends them (Profiler.h): [{label, depth, total_us|total_ms,
 * ...}, ...], root first.
 */
(function (root) {
	'use strict';

	// Phases drawn in the stacked chart, in a fixed order with a fixed colour each; the rest are drawn as "other"
	var PHASES = [
		{ key: 'packets', label: 'Packets', color: 'var(--tr-1)' },
		{ key: 'entities', label: 'Entities', color: 'var(--tr-2)' },
		{ key: 'physics', label: 'Physics', color: 'var(--tr-3)' },
		{ key: 'web', label: 'Web requests', color: 'var(--tr-3)' }, // only the dashboard and UGC server, which have no physics
		{ key: 'replica', label: 'Replica', color: 'var(--tr-4)' },
		{ key: 'database', label: 'Database', color: 'var(--tr-5)' },
		{ key: 'cdclient', label: 'CDClient', color: 'var(--tr-6)' },
	];
	var OTHER_PHASES = ['other', 'scripts', 'log_flush'];

	function num(v) { return typeof v === 'number' && isFinite(v) ? v : 0; }

	// The index of each node's parent (-1 for the root)
	function parents(nodes) {
		var out = [], stack = [];
		for (var i = 0; i < nodes.length; i++) {
			var d = nodes[i].depth;
			stack.length = d;
			out.push(d > 0 && stack.length ? stack[d - 1] : -1);
			stack[d] = i;
		}
		return out;
	}

	/**
	 * Flame graph rectangles: x and w as fractions of the focused node's width (its ancestors span the whole width),
	 * row = depth. Children are laid out left to right in the order they come. Nodes narrower than minWidth are left out.
	 */
	function flameLayout(nodes, focus, minWidth) {
		focus = focus || 0;
		minWidth = minWidth || 0;
		if (!nodes.length) return [];
		var parent = parents(nodes), total = function (i) { return num(nodes[i].total_us); };
		var rects = [];
		// Ancestors of the focus, full width
		var chain = [];
		for (var a = parent[focus]; a >= 0; a = parent[a]) chain.unshift(a);
		chain.forEach(function (i) { rects.push({ index: i, x: 0, w: 1, row: nodes[i].depth, ancestor: true }); });
		var focusTotal = total(focus) || 1;
		var x = {};
		x[focus] = 0;
		rects.push({ index: focus, x: 0, w: 1, row: nodes[focus].depth });
		var next = {}; // where the next child of a node starts
		next[focus] = 0;
		for (var i = focus + 1; i < nodes.length && nodes[i].depth > nodes[focus].depth; i++) {
			var p = parent[i];
			if (!(p in next)) continue; // under a node too narrow to draw
			var w = total(i) / focusTotal;
			var start = next[p];
			next[p] = start + w;
			if (w < minWidth) continue;
			next[i] = start;
			rects.push({ index: i, x: start, w: w, row: nodes[i].depth });
		}
		return rects;
	}

	/**
	 * A slow frame's timeline: each scope from its first start for its total time, row = depth, as fractions of the
	 * frame. A scope entered many times is drawn as one bar of all of its time (count says how many).
	 */
	function timelineLayout(scopes, durationMs) {
		var d = num(durationMs) || 1;
		return scopes.map(function (s, i) {
			var x = Math.min(Math.max(num(s.start_ms) / d, 0), 1);
			return { index: i, x: x, w: Math.max(Math.min(num(s.total_ms) / d, 1 - x), 0), row: s.depth };
		});
	}

	/**
	 * The stacked phase series: the drawn phases (those with any time) and "other" for the rest, each {key, label,
	 * color, values}; values are milliseconds per second (null where nothing was reported)
	 */
	function phaseSeries(phases) {
		phases = phases || {};
		var length = 0;
		Object.keys(phases).forEach(function (k) { length = Math.max(length, (phases[k] || []).length); });
		var out = [];
		PHASES.forEach(function (p) {
			var values = phases[p.key] || [];
			if (!values.some(function (v) { return num(v) > 0; })) return;
			out.push({ key: p.key, label: p.label, color: p.color, values: values.slice() });
		});
		var other = [];
		for (var i = 0; i < length; i++) {
			var sum = null;
			OTHER_PHASES.forEach(function (k) { var v = (phases[k] || [])[i]; if (v != null) sum = (sum || 0) + num(v); });
			other.push(sum);
		}
		if (other.some(function (v) { return num(v) > 0; })) out.push({ key: 'other', label: 'Other', color: 'var(--tr-other)', values: other });
		return out;
	}

	// ---- Drawing (browser only) ----

	var SVG = 'http://www.w3.org/2000/svg';
	function el(name, attrs, parent) {
		var e = document.createElementNS(SVG, name);
		for (var k in attrs) e.setAttribute(k, attrs[k]);
		if (parent) parent.appendChild(e);
		return e;
	}
	function esc(s) { return String(s).replace(/[&<>"']/g, function (c) { return { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]; }); }
	function niceMax(v) {
		if (v <= 0) return 1;
		var p = Math.pow(10, Math.floor(Math.log10(v))), n = v / p;
		return (n <= 1 ? 1 : n <= 2 ? 2 : n <= 5 ? 5 : 10) * p;
	}
	function ms(v) { return v == null ? '-' : v >= 1000 ? (v / 1000).toFixed(2) + ' s' : (v >= 10 ? v.toFixed(0) : v.toFixed(2)) + ' ms'; }

	function tip(box) {
		var t = box.querySelector('.pf-tip');
		if (!t) { t = document.createElement('div'); t.className = 'pf-tip d-none'; box.appendChild(t); }
		return t;
	}
	function showTip(box, html, x, y) {
		var t = tip(box);
		t.innerHTML = html;
		t.classList.remove('d-none');
		var left = Math.min(x + 12, box.clientWidth - t.offsetWidth - 4);
		t.style.left = Math.max(left, 0) + 'px';
		t.style.top = Math.max(y - t.offsetHeight - 8, 0) + 'px';
	}
	function hideTip(box) { tip(box).classList.add('d-none'); }

	/**
	 * A time chart: lines ([{label, color, values, dash}]) or a stack of areas (stacked: true), with a crosshair and
	 * every series' value in the tooltip; `threshold` draws a dashed line (the slow frame threshold)
	 */
	function timeChart(svg, times, series, options) {
		options = options || {};
		var box = svg.parentNode;
		var width = box.clientWidth || 600, height = box.clientHeight || 180, left = 52, bottom = 20, top = 8, right = 8;
		svg.innerHTML = '';
		svg.setAttribute('viewBox', '0 0 ' + width + ' ' + height);
		var n = times.length, max = 0;
		var stacks = [];
		for (var i = 0; i < n; i++) {
			var sum = 0;
			series.forEach(function (s) {
				var v = s.values[i];
				if (options.stacked) sum += num(v); else if (v != null) max = Math.max(max, v);
			});
			stacks.push(sum);
			if (options.stacked) max = Math.max(max, sum);
		}
		if (options.threshold) max = Math.max(max, Math.min(options.threshold * 1.1, max * 4 || options.threshold * 1.1));
		max = niceMax(max);
		var W = width - left - right, H = height - top - bottom;
		var X = function (i) { return left + (n > 1 ? i / (n - 1) : 0) * W; };
		var Y = function (v) { return top + H - Math.min(v / max, 1) * H; };
		for (var g = 0; g <= 4; g++) {
			var v = max * g / 4, y = Y(v);
			el('line', { x1: left, x2: width - right, y1: y, y2: y, class: 'grid' }, svg);
			el('text', { x: left - 6, y: y + 4, 'text-anchor': 'end', class: 'axis' }, svg).textContent = options.format ? options.format(v) : String(v);
		}
		[0, Math.floor((n - 1) / 2), n - 1].forEach(function (i) {
			if (i < 0 || !times.length) return;
			el('text', { x: X(i), y: height - 4, 'text-anchor': i === 0 ? 'start' : i === n - 1 ? 'end' : 'middle', class: 'axis' }, svg).textContent =
				new Date(times[i] * 1000).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: n <= 400 ? '2-digit' : undefined });
		});
		if (options.stacked) {
			var base = new Array(n).fill(0);
			series.forEach(function (s) {
				var upper = [], lower = [];
				for (var i = 0; i < n; i++) {
					if (s.values[i] == null) continue;
					lower.push(X(i) + ',' + Y(base[i]));
					base[i] += num(s.values[i]);
					upper.push(X(i) + ',' + Y(base[i]));
				}
				if (!upper.length) return;
				el('polygon', { points: upper.concat(lower.reverse()).join(' '), fill: s.color, class: 'area' }, svg);
			});
		} else {
			series.forEach(function (s) {
				var d = '', pen = false;
				for (var i = 0; i < n; i++) {
					var v = s.values[i];
					if (v == null) { pen = false; continue; }
					d += (pen ? 'L' : 'M') + X(i).toFixed(1) + ',' + Y(v).toFixed(1);
					pen = true;
				}
				if (d) el('path', { d: d, stroke: s.color, class: 'line' + (s.dash ? ' dashed' : '') }, svg);
			});
		}
		if (options.threshold && options.threshold <= max) {
			var ty = Y(options.threshold);
			el('line', { x1: left, x2: width - right, y1: ty, y2: ty, class: 'threshold' }, svg);
			el('text', { x: width - right, y: ty - 3, 'text-anchor': 'end', class: 'axis' }, svg).textContent = 'slow ' + ms(options.threshold);
		}
		var cross = el('line', { y1: top, y2: top + H, class: 'cross', visibility: 'hidden' }, svg);
		var hit = el('rect', { x: left, y: top, width: W, height: H, fill: 'transparent' }, svg);
		hit.addEventListener('mousemove', function (e) {
			var r = svg.getBoundingClientRect(), px = (e.clientX - r.left) * width / r.width;
			var i = Math.round((px - left) / W * (n - 1));
			if (i < 0 || i >= n) return;
			cross.setAttribute('x1', X(i)); cross.setAttribute('x2', X(i)); cross.setAttribute('visibility', 'visible');
			var rows = series.map(function (s) {
				return '<div><span class="pf-key" style="background:' + s.color + '"></span>' + esc(s.label) + ': ' + (options.format ? options.format(s.values[i]) : esc(s.values[i])) + '</div>';
			});
			if (options.extra) rows = rows.concat(options.extra(i));
			showTip(box, '<div class="fw-semibold">' + esc(new Date(times[i] * 1000).toLocaleTimeString()) + '</div>' + rows.join(''), e.clientX - box.getBoundingClientRect().left, e.clientY - box.getBoundingClientRect().top);
		});
		hit.addEventListener('mouseleave', function () { cross.setAttribute('visibility', 'hidden'); hideTip(box); });
	}

	/**
	 * Rows of named bars (a flame graph or a timeline): rects from flameLayout or timelineLayout; `describe(index)` is
	 * the tooltip; `onClick(index)` for zooming
	 */
	function bars(svg, rects, nodes, options) {
		options = options || {};
		var box = svg.parentNode, row = 20, width = box.clientWidth || 800;
		var rows = rects.reduce(function (m, r) { return Math.max(m, r.row + 1); }, 1);
		var height = rows * row + 2;
		svg.innerHTML = '';
		svg.setAttribute('viewBox', '0 0 ' + width + ' ' + height);
		svg.setAttribute('height', height);
		rects.forEach(function (r) {
			var node = nodes[r.index], x = r.x * width, w = Math.max(r.w * width - 1, 1), y = r.row * row;
			var g = el('g', { class: 'pf-bar' + (r.ancestor ? ' ancestor' : ''), 'data-index': r.index }, svg);
			el('rect', { x: x, y: y, width: w, height: row - 2, rx: 2, class: 'tone-' + (hash(node.label) % 3) }, g);
			if (w > 30) {
				var text = el('text', { x: x + 4, y: y + 14, class: 'pf-label' }, g);
				var chars = Math.floor((w - 8) / 6.5);
				text.textContent = node.label.length > chars ? node.label.slice(0, Math.max(chars - 1, 1)) + '…' : node.label;
			}
			g.addEventListener('mousemove', function (e) {
				var b = box.getBoundingClientRect();
				showTip(box, options.describe ? options.describe(r.index) : esc(node.label), e.clientX - b.left, e.clientY - b.top);
			});
			g.addEventListener('mouseleave', function () { hideTip(box); });
			if (options.onClick) g.addEventListener('click', function () { options.onClick(r.index); });
		});
	}

	function hash(s) {
		var h = 0;
		for (var i = 0; i < s.length; i++) h = (h * 31 + s.charCodeAt(i)) | 0;
		return Math.abs(h);
	}

	root.PerfCharts = {
		PHASES: PHASES, parents: parents, flameLayout: flameLayout, timelineLayout: timelineLayout, phaseSeries: phaseSeries,
		timeChart: timeChart, bars: bars, ms: ms, esc: esc,
	};
})(typeof window !== 'undefined' ? window : this);
