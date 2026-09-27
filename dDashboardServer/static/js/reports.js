/**
 * Economy reports page: coin, U-score and item flows, player activity, the world map, transfers, item tracing and
 * duplicate scans.
 * Charts are plain SVG; every chart has a table view next to it.
 */
(function () {
	'use strict';

	var DAY_MS = 24 * 60 * 60 * 1000;
	var REFRESH_MS = 2000; // at most this often, however many worlds report in
	var SVG_NS = 'http://www.w3.org/2000/svg';

	var state = { days: 30, staff: document.getElementById('includeStaff').checked, today: 0, sources: {}, mapKinds: [], stats: [], powerupKinds: {}, lot: 0, lotName: '' };
	var nf = new Intl.NumberFormat();
	var compact = new Intl.NumberFormat(undefined, { notation: 'compact', maximumFractionDigits: 1 });

	function num(n) { return nf.format(n || 0); }
	function dayDate(day) { return new Date(day * DAY_MS); }
	function dayLabel(day, withYear) {
		return dayDate(day).toLocaleDateString(undefined, withYear ? { year: 'numeric', month: 'short', day: 'numeric', timeZone: 'UTC' } : { month: 'short', day: 'numeric', timeZone: 'UTC' });
	}
	function sourceName(id) {
		var name = state.sources[String(id)] || ('Source ' + id);
		return name.charAt(0) + name.slice(1).toLowerCase().replace(/_/g, ' ');
	}
	function range() {
		return { from: state.today - (state.days - 1), to: state.today };
	}
	function query(extra) {
		var r = range();
		var q = '?from=' + r.from + '&to=' + r.to + (state.staff ? '&staff=1' : '');
		return q + (extra || '');
	}
	function charLink(id, name) {
		if (!id || id === '0') return '<span class="text-body-secondary">-</span>';
		return fmt.link('/characters/' + id, name || id);
	}

	// ---- Aggregation ----

	// rows: [{day, source, <up>, <down>}] -> per-day totals over the whole range (missing days are zero), per-source totals
	function summarize(rows, up, down) {
		var r = range();
		var byDay = {};
		var bySource = {};
		var total = { up: 0, down: 0 };
		rows.forEach(function (row) {
			var d = byDay[row.day] || (byDay[row.day] = { up: 0, down: 0 });
			var s = bySource[row.source] || (bySource[row.source] = { up: 0, down: 0 });
			d.up += row[up]; d.down += row[down];
			s.up += row[up]; s.down += row[down];
			total.up += row[up]; total.down += row[down];
		});
		var days = [];
		for (var day = r.from; day <= r.to; day++) {
			var v = byDay[day] || { up: 0, down: 0 };
			days.push({ day: day, up: v.up, down: v.down });
		}
		var sources = Object.keys(bySource).map(function (k) { return { source: k, up: bySource[k].up, down: bySource[k].down }; });
		sources.sort(function (a, b) { return (b.up + b.down) - (a.up + a.down); });
		return { days: days, sources: sources, total: total };
	}

	// Reports leave staff out by default, so a server where only staff have played looks empty.
	// When a flow comes back empty, ask again with staff included and say how much is hidden.
	function withHiddenStaff(summary, url, up, down) {
		if (state.staff || summary.total.up || summary.total.down) return Promise.resolve(summary);
		return api.get(url + '&staff=1').then(function (res) {
			var staff = summarize(res.rows || [], up, down).total;
			summary.staffHidden = staff.up + staff.down ? staff : null;
			return summary;
		}).catch(function () { return summary; });
	}

	function staffHint(summary, labels) {
		var s = summary.staffHidden;
		if (!s) return '';
		return ' Staff activity is hidden (' + esc(labels.up.toLowerCase()) + ' ' + num(s.up) + ', ' + esc(labels.down.toLowerCase()) + ' ' + num(s.down) + ').' +
			' <button type="button" class="btn btn-link btn-sm p-0 align-baseline" data-include-staff>Include staff</button>';
	}

	// ---- Stat tiles ----

	function tiles(el, labels, total) {
		var net = total.up - total.down;
		var items = [
			{ label: labels.up, value: num(total.up) },
			{ label: labels.down, value: num(total.down) },
			{ label: 'Net change', value: (net > 0 ? '+' : '') + num(net) }
		];
		el.innerHTML = items.map(function (t) {
			return '<div class="col-sm-4"><div class="card h-100"><div class="card-body">' +
				'<div class="small text-body-secondary">' + esc(t.label) + '</div>' +
				'<div class="viz-hero">' + esc(t.value) + '</div></div></div></div>';
		}).join('');
	}

	// ---- Tables ----

	function table(el, headers, rows, empty) {
		if (!rows.length) { el.innerHTML = '<p class="text-body-secondary mb-0">' + esc(empty || 'Nothing recorded in this range.') + '</p>'; return; }
		el.innerHTML = '<table class="table table-sm table-hover align-middle mb-0"><thead><tr>' +
			headers.map(function (h) { return '<th' + (h.num ? ' class="text-end"' : '') + '>' + esc(h.label) + '</th>'; }).join('') +
			'</tr></thead><tbody>' +
			rows.map(function (cells) {
				return '<tr>' + cells.map(function (c, i) { return '<td' + (headers[i].num ? ' class="text-end viz-num"' : '') + '>' + c + '</td>'; }).join('') + '</tr>';
			}).join('') + '</tbody></table>';
	}

	function dailyTable(el, summary, labels) {
		var rows = summary.days.filter(function (d) { return d.up || d.down; }).reverse().map(function (d) {
			return [esc(dayLabel(d.day, true)), num(d.up), num(d.down), num(d.up - d.down)];
		});
		table(el, [{ label: 'Day' }, { label: labels.up, num: true }, { label: labels.down, num: true }, { label: 'Net', num: true }], rows);
	}

	function sourceTable(el, summary, labels) {
		table(el, [{ label: 'Source' }, { label: labels.up, num: true }, { label: labels.down, num: true }],
			summary.sources.map(function (s) { return [esc(sourceName(s.source)), num(s.up), num(s.down)]; }));
	}

	// ---- Chart: gains above the baseline, losses below it, one shared axis ----

	var tooltip = document.getElementById('vizTooltip');

	function showTip(evt, html) {
		tooltip.innerHTML = html;
		tooltip.hidden = false;
		var pad = 12;
		var w = tooltip.offsetWidth, h = tooltip.offsetHeight;
		var x = evt.clientX + pad, y = evt.clientY + pad;
		if (x + w > window.innerWidth - 8) x = evt.clientX - w - pad;
		if (y + h > window.innerHeight - 8) y = evt.clientY - h - pad;
		tooltip.style.left = x + 'px';
		tooltip.style.top = y + 'px';
	}
	function hideTip() { tooltip.hidden = true; }

	function niceStep(max, ticks) {
		if (max <= 0) return 1;
		var raw = max / ticks;
		var mag = Math.pow(10, Math.floor(Math.log10(raw)));
		var n = raw / mag;
		return (n <= 1 ? 1 : n <= 2 ? 2 : n <= 5 ? 5 : 10) * mag;
	}

	function el(name, attrs, parent) {
		var node = document.createElementNS(SVG_NS, name);
		Object.keys(attrs).forEach(function (k) { node.setAttribute(k, attrs[k]); });
		if (parent) parent.appendChild(node);
		return node;
	}

	// Bar with a 4px rounded end away from the baseline and a square end on it
	function barPath(x, w, base, end) {
		var h = Math.abs(end - base);
		if (h < 0.5) return '';
		var r = Math.min(4, w / 2, h);
		var up = end < base;
		var tip = end, s = up ? 1 : -1;
		return 'M' + x + ',' + base +
			'V' + (tip + s * r) +
			'Q' + x + ',' + tip + ' ' + (x + r) + ',' + tip +
			'H' + (x + w - r) +
			'Q' + (x + w) + ',' + tip + ' ' + (x + w) + ',' + (tip + s * r) +
			'V' + base + 'Z';
	}

	// Charts without labels.down have one series, drawn above the baseline
	function legend(name, labels) {
		var box = document.querySelector('[data-legend="' + name + '"]');
		if (!box) return;
		box.innerHTML = '<span class="viz-key"><span class="viz-swatch viz-up"></span>' + esc(labels.up) + '</span>' +
			(labels.down ? '<span class="viz-key"><span class="viz-swatch viz-down"></span>' + esc(labels.down) + '</span>' : '');
	}

	function chart(container, summary, labels) {
		container.innerHTML = '';
		var days = summary.days;
		var maxUp = 0, maxDown = 0;
		days.forEach(function (d) { maxUp = Math.max(maxUp, d.up); maxDown = Math.max(maxDown, d.down); });
		if (!maxUp && !maxDown) {
			container.innerHTML = '<p class="text-body-secondary mb-0">Nothing recorded in this range.' + staffHint(summary, labels) + '</p>';
			return;
		}

		var width = Math.max(container.clientWidth, 280);
		var height = 260;
		var m = { top: 12, right: 8, bottom: 26, left: 56 };
		var plotW = width - m.left - m.right, plotH = height - m.top - m.bottom;

		var step = niceStep(Math.max(maxUp, maxDown), 3);
		var top = Math.ceil(maxUp / step) * step;
		var bottom = Math.ceil(maxDown / step) * step;
		var span = (top + bottom) || 1;
		var y = function (v) { return m.top + (top - v) / span * plotH; };
		var base = y(0);

		var svg = el('svg', { width: width, height: height, viewBox: '0 0 ' + width + ' ' + height, role: 'img',
			'aria-label': labels.title + ' per day' + (labels.down ? ', ' + labels.up.toLowerCase() + ' above the line and ' + labels.down.toLowerCase() + ' below' : '') }, container);

		for (var v = -bottom; v <= top; v += step) {
			var gy = y(v);
			el('line', { x1: m.left, x2: width - m.right, y1: gy, y2: gy, class: v === 0 ? 'viz-baseline' : 'viz-grid' }, svg);
			var t = el('text', { x: m.left - 8, y: gy + 4, 'text-anchor': 'end', class: 'viz-tick' }, svg);
			t.textContent = compact.format(Math.abs(v));
		}

		var band = plotW / days.length;
		var barW = Math.max(1, Math.min(24, band - 2)); // 2px surface gap between neighbours
		var labelEvery = Math.ceil(days.length / Math.max(2, Math.floor(plotW / 64)));

		days.forEach(function (d, i) {
			var x = m.left + i * band + (band - barW) / 2;
			if (d.up) el('path', { d: barPath(x, barW, base, y(d.up)), class: 'viz-up' }, svg);
			if (d.down) el('path', { d: barPath(x, barW, base, y(-d.down)), class: 'viz-down' }, svg);

			if (i % labelEvery === 0) {
				var lbl = el('text', { x: m.left + i * band + band / 2, y: height - 8, 'text-anchor': 'middle', class: 'viz-tick' }, svg);
				lbl.textContent = dayLabel(d.day);
			}

			// Hit target covers the whole day column, not just the bars
			var hit = el('rect', { x: m.left + i * band, y: m.top, width: band, height: plotH, class: 'viz-hit' }, svg);
			hit.addEventListener('mousemove', function (evt) {
				hit.classList.add('viz-hover');
				showTip(evt, '<div class="fw-semibold mb-1">' + esc(dayLabel(d.day, true)) + '</div>' +
					'<div class="viz-tip-row"><span class="viz-swatch viz-up"></span>' + esc(labels.up) + '<span class="ms-auto viz-num">' + num(d.up) + '</span></div>' +
					(labels.down ? '<div class="viz-tip-row"><span class="viz-swatch viz-down"></span>' + esc(labels.down) + '<span class="ms-auto viz-num">' + num(d.down) + '</span></div>' +
						'<div class="viz-tip-row text-body-secondary">Net<span class="ms-auto viz-num">' + num(d.up - d.down) + '</span></div>' : ''));
			});
			hit.addEventListener('mouseleave', function () { hit.classList.remove('viz-hover'); hideTip(); });
		});
	}

	// ---- Flow sections (coins, U-score, items share one shape) ----

	var charts = {};

	function flowSection(name, summary, labels, ids) {
		tiles(document.getElementById(ids.tiles), labels, summary.total);
		legend(name, labels);
		charts[name] = function () { chart(document.getElementById(ids.chart), summary, labels); };
		charts[name]();
		dailyTable(document.getElementById(ids.daily), summary, labels);
		sourceTable(document.getElementById(ids.sources), summary, labels);
	}

	var COIN_LABELS = { title: 'Coins', up: 'Coins gained', down: 'Coins spent' };
	var USCORE_LABELS = { title: 'U-score', up: 'U-score gained', down: 'U-score lost' };
	var ITEM_LABELS = { title: 'Items', up: 'Items created', down: 'Items destroyed' };

	function loadCoins() {
		var url = '/api/reports/currency' + query();
		return Promise.all([api.get(url), api.get('/api/reports/top_earners' + query('&limit=25'))]).then(function (res) {
			return withHiddenStaff(summarize(res[0].rows || [], 'gained', 'spent'), url, 'gained', 'spent').then(function (summary) { return [summary, res[1]]; });
		}).then(function (res) {
			flowSection('coins', res[0], COIN_LABELS,
				{ tiles: 'coinTiles', chart: 'coinChart', daily: 'coinDaily', sources: 'coinSources' });
			table(document.getElementById('topEarners'), [{ label: 'Character' }, { label: 'Earned', num: true }, { label: 'Spent', num: true }],
				(res[1].rows || []).map(function (r) { return [charLink(r.character_id, r.name), num(r.gained), num(r.spent)]; }));
		});
	}

	function loadUScore() {
		var url = '/api/reports/uscore' + query();
		return api.get(url).then(function (res) {
			return withHiddenStaff(summarize(res.rows || [], 'gained', 'lost'), url, 'gained', 'lost');
		}).then(function (summary) {
			flowSection('uscore', summary, USCORE_LABELS,
				{ tiles: 'uscoreTiles', chart: 'uscoreChart', daily: 'uscoreDaily', sources: 'uscoreSources' });
		});
	}

	function itemLink(lot, name) {
		return '<a href="#" data-lot="' + esc(lot) + '">' + esc(name || ('LOT ' + lot)) + '</a> <span class="text-body-secondary small">' + esc(lot) + '</span>';
	}

	// Who holds the selected item: a background scan over every character, so only run when an item is picked
	var holders = { lot: 0, data: null, csvUrl: null };

	function showHolders(h) {
		var side = document.getElementById('itemSide');
		document.getElementById('itemSideTitle').textContent = 'Held now: ' + num(h.total) + ' by ' + num(h.holderCount) + ' character' + (h.holderCount === 1 ? '' : 's');
		table(side, [{ label: 'Character' }, { label: 'Where' }, { label: 'Count', num: true }],
			(h.holders || []).slice(0, 500).map(function (r) {
				var places = Object.keys(r.places || {}).map(function (p) { return esc(p.toLowerCase().replace(/_/g, ' ')) + ' ' + num(r.places[p]); }).join(', ');
				return [charLink(r.character_id, r.character_name), '<span class="small text-body-secondary">' + places + '</span>', num(r.count)];
			}), 'Nobody holds this item.');
		// The CSV is built here from the scan's result rather than scanning again on the server
		if (holders.csvUrl) URL.revokeObjectURL(holders.csvUrl);
		holders.csvUrl = URL.createObjectURL(new Blob(['\ufeff' + toCsv(h.holders || [], [['character_id', 'Character ID'], ['character_name', 'Character'], ['count', 'Count']])], { type: 'text/csv' }));
		updateCsvLinks();
	}

	function loadHolders(lot) {
		holders = { lot: lot, data: null, csvUrl: holders.csvUrl };
		document.getElementById('itemSideTitle').textContent = 'Held now';
		document.getElementById('itemSide').innerHTML = '<p class="text-body-secondary mb-0">Counting who holds it (reads every character)...</p>';
		api.job('/api/reports/holders/' + lot, null, 'GET').then(function (h) {
			if (holders.lot !== lot) return; // another item was picked meanwhile
			holders.data = h;
			showHolders(h);
		}).catch(function () {
			document.getElementById('itemSide').innerHTML = '<p class="text-body-secondary mb-0">Could not count holders.</p>';
		});
	}

	function loadItems(refreshHolders) {
		var lotQuery = state.lot ? '&lot=' + state.lot : '';
		var url = '/api/reports/items' + query(lotQuery);
		var requests = [api.get(url)];
		if (!state.lot) requests.push(api.get('/api/reports/top_items' + query('&limit=25')));
		else if (refreshHolders || holders.lot !== state.lot) loadHolders(state.lot);
		return Promise.all(requests).then(function (res) {
			return withHiddenStaff(summarize(res[0].rows || [], 'created', 'destroyed'), url, 'created', 'destroyed').then(function (summary) { res.push(summary); return res; });
		}).then(function (res) {
			var summary = res[res.length - 1];
			state.lotName = res[0].name || '';
			document.getElementById('itemTitle').textContent = state.lot ? (state.lotName || 'LOT ' + state.lot) + ' (' + state.lot + ')' : 'All items';
			flowSection('items', summary, ITEM_LABELS,
				{ tiles: 'itemTiles', chart: 'itemChart', daily: 'itemDaily', sources: 'itemSources' });

			var side = document.getElementById('itemSide');
			if (state.lot) {
				if (holders.lot === state.lot && holders.data) showHolders(holders.data);
			} else {
				document.getElementById('itemSideTitle').textContent = 'Most created';
				table(side, [{ label: 'Item' }, { label: 'Created', num: true }, { label: 'Destroyed', num: true }],
					(res[1].rows || []).map(function (r) { return [itemLink(r.lot, r.name), num(r.created), num(r.destroyed)]; }));
			}
		});
	}

	function setLot(lot) {
		state.lot = lot > 0 ? lot : 0;
		document.getElementById('itemLot').value = state.lot ? String(state.lot) : '';
		loadItems(true).then(updateCsvLinks);
	}

	// ---- Activity: world events and player statistics per day ----

	// Metrics are map event kinds ('map:<value>'), player statistics ('stat:<value>') and powerup types ('pu:<kind>:<type>'),
	// all named by the server. place: where (see /api/reports/places): '' everywhere, 'properties', '<zone>', '<zone>:<clone>', '*:<clone>'
	var activity = { data: null, metric: '', loaded: false, place: '', places: [] };

	function mapKind(value) {
		return state.mapKinds.filter(function (k) { return k.value === value; })[0];
	}

	// What a map event row counts for its kind: coins or items when the kind has a quantity, otherwise events
	function mapAmount(kind, row) {
		return kind && kind.quantity ? row.quantity : row.events;
	}

	function parseMetric(key) {
		var parts = key.split(':');
		return { map: parts[0] === 'map', powerup: parts[0] === 'pu', value: parseInt(parts[1], 10), type: parts.slice(2).join(':') };
	}

	// Every metric with something recorded in the range: {key, group, name, total}
	function activityMetrics(data) {
		var totals = {};
		data.mapDays.forEach(function (r) { totals['map:' + r.kind] = (totals['map:' + r.kind] || 0) + mapAmount(mapKind(r.kind), r); });
		data.statDays.forEach(function (r) { totals['stat:' + r.stat] = (totals['stat:' + r.stat] || 0) + r.amount; });
		var powerups = [];
		(data.powerupDays || []).forEach(function (r) {
			var key = 'pu:' + r.kind + ':' + r.type;
			if (totals[key] === undefined) {
				var kind = mapKind(r.kind);
				powerups.push({ key: key, group: 'Powerups', name: (kind ? kind.name : 'Powerups') + ': ' + r.type });
			}
			totals[key] = (totals[key] || 0) + r.events;
		});
		var metrics = state.mapKinds.map(function (k) {
			return { key: 'map:' + k.value, group: 'World events', name: k.name + (k.quantity ? ' (' + k.quantity.toLowerCase() + ')' : '') };
		}).concat(powerups).concat(state.stats.map(function (st) { return { key: 'stat:' + st.value, group: 'Player statistics', name: st.name }; }));
		metrics.forEach(function (m) { m.total = totals[m.key] || 0; });
		return metrics.filter(function (m) { return m.total; });
	}

	// One metric as a flow summary, so the shared chart draws it
	function activitySummary(data, key) {
		var metric = parseMetric(key);
		var byDay = {};
		if (metric.map) {
			data.mapDays.forEach(function (r) { if (r.kind === metric.value) byDay[r.day] = (byDay[r.day] || 0) + mapAmount(mapKind(r.kind), r); });
		} else if (metric.powerup) {
			(data.powerupDays || []).forEach(function (r) { if (r.kind === metric.value && r.type === metric.type) byDay[r.day] = (byDay[r.day] || 0) + r.events; });
		} else {
			data.statDays.forEach(function (r) { if (r.stat === metric.value) byDay[r.day] = (byDay[r.day] || 0) + r.amount; });
		}
		var r = range(), days = [], total = 0;
		for (var day = r.from; day <= r.to; day++) {
			days.push({ day: day, up: byDay[day] || 0, down: 0 });
			total += byDay[day] || 0;
		}
		return { days: days, total: { up: total, down: 0 } };
	}

	// Rows per zone and clone for a metric: {zone, name, amount, property, place, property_name, unknown, owner_*}
	function activityRows(data, key) {
		var metric = parseMetric(key);
		if (metric.map) {
			return data.mapZones.filter(function (z) { return z.kind === metric.value; })
				.map(function (z) { return $.extend({}, z, { amount: mapAmount(mapKind(z.kind), z) }); });
		}
		if (metric.powerup) return [];
		return data.statZones.filter(function (z) { return z.stat === metric.value; });
	}

	// Worlds as they are; property instances collapsed into "All properties" and one row per property zone, with its
	// busiest properties under it (every property is in the Where picker)
	var PROPERTIES_PER_ZONE = 5;
	function activityZoneRows(rows) {
		var worlds = {}, zones = {}, all = 0, out = [];
		rows.forEach(function (r) {
			if (!r.property) {
				worlds[r.zone] = worlds[r.zone] || { zone: r.zone, name: r.name, amount: 0 };
				worlds[r.zone].amount += r.amount;
				return;
			}
			all += r.amount;
			var z = zones[r.zone] = zones[r.zone] || { zone: r.zone, name: r.name, amount: 0, properties: [] };
			z.amount += r.amount;
			z.properties.push(r);
		});
		Object.keys(worlds).forEach(function (k) { out.push({ amount: worlds[k].amount, cells: [esc(worlds[k].name)], place: String(worlds[k].zone) }); });
		var zoneList = Object.keys(zones).map(function (k) { return zones[k]; });
		if (zoneList.length) {
			out.push({ amount: all, cells: ['<span class="fw-semibold">All properties</span>'], place: 'properties', group: true });
		}
		out.sort(function (a, b) { return b.amount - a.amount; });
		zoneList.sort(function (a, b) { return b.amount - a.amount; }).forEach(function (z) {
			out.push({ amount: z.amount, cells: ['<span class="ms-2">' + esc(z.name) + ' properties</span>'], place: String(z.zone) });
			z.properties.sort(function (a, b) { return b.amount - a.amount; });
			z.properties.slice(0, PROPERTIES_PER_ZONE).forEach(function (p) {
				out.push({ amount: p.amount, cells: ['<span class="ms-4 small' + (p.unknown ? ' text-body-secondary fst-italic' : '') + '">' + esc(p.property_name) + '</span>'], place: p.place });
			});
			if (z.properties.length > PROPERTIES_PER_ZONE) {
				out.push({ amount: null, cells: ['<span class="ms-4 small text-body-secondary">and ' + num(z.properties.length - PROPERTIES_PER_ZONE) + ' more (pick them under Where)</span>'] });
			}
		});
		return out;
	}

	function placeLink(place, html) {
		return place === undefined ? html : '<a href="#" data-place="' + esc(place) + '">' + html + '</a>';
	}

	function renderActivityPowerups(data) {
		var byType = {};
		(data.powerupDays || []).forEach(function (r) {
			var t = byType[r.type] = byType[r.type] || { type: r.type, drops: 0, pickups: 0 };
			if (r.kind === state.powerupKinds.drops) t.drops += r.events;
			else t.pickups += r.events;
		});
		var types = Object.keys(byType).map(function (k) { return byType[k]; }).sort(function (a, b) { return b.drops - a.drops; });
		table(document.getElementById('activityPowerups'), [{ label: 'Restores' }, { label: 'Dropped', num: true }, { label: 'Picked up', num: true }],
			types.map(function (t) { return [esc(t.type), num(t.drops), num(t.pickups)]; }), 'No powerups recorded here in this range.');
	}

	function renderActivity() {
		var data = activity.data;
		var metrics = activityMetrics(data);
		if (!metrics.some(function (m) { return m.key === activity.metric; })) activity.metric = metrics.length ? metrics[0].key : '';
		var select = document.getElementById('activityMetric');
		var groups = [];
		metrics.forEach(function (m) { if (groups.indexOf(m.group) < 0) groups.push(m.group); });
		select.innerHTML = metrics.length ? groups.map(function (g) {
			return '<optgroup label="' + esc(g) + '">' + metrics.filter(function (m) { return m.group === g; }).map(function (m) {
				return '<option value="' + esc(m.key) + '"' + (m.key === activity.metric ? ' selected' : '') + '>' + esc(m.name) + '</option>';
			}).join('') + '</optgroup>';
		}).join('') : '<option value="">Nothing recorded</option>';
		select.disabled = !metrics.length;
		renderActivityPowerups(data);

		table(document.getElementById('activityTotals'), [{ label: 'What' }, { label: 'Total', num: true }, { label: 'Per day', num: true }],
			metrics.map(function (m) {
				return ['<a href="#" data-metric="' + esc(m.key) + '"' + (m.key === activity.metric ? ' class="fw-semibold"' : '') + '>' + esc(m.name) + '</a>' +
					' <span class="small text-body-secondary">' + esc(m.group) + '</span>', num(m.total), m.total && m.total / state.days < 0.1 ? '&lt;0.1' : num(Math.round(m.total / state.days * 10) / 10)];
			}));

		var metric = metrics.filter(function (m) { return m.key === activity.metric; })[0];
		if (!metric) {
			delete charts.activity;
			document.querySelector('[data-legend="activity"]').innerHTML = '';
			document.getElementById('activityChart').innerHTML = '<p class="text-body-secondary mb-0">Nothing recorded in this range.</p>';
			document.getElementById('activityDaily').innerHTML = '';
			document.getElementById('activityZones').innerHTML = '<p class="text-body-secondary mb-0">Nothing recorded in this range.</p>';
			return;
		}
		var labels = { title: metric.name, up: metric.name };
		var summary = activitySummary(data, activity.metric);
		legend('activity', labels);
		charts.activity = function () { chart(document.getElementById('activityChart'), summary, labels); };
		charts.activity();
		table(document.getElementById('activityDaily'), [{ label: 'Day' }, { label: metric.name, num: true }],
			summary.days.filter(function (d) { return d.up; }).reverse().map(function (d) { return [esc(dayLabel(d.day, true)), num(d.up)]; }));
		document.getElementById('activityZonesTitle').textContent = metric.name + ' by world';
		var zoneRows = activityZoneRows(activityRows(data, activity.metric));
		table(document.getElementById('activityZones'), [{ label: 'World' }, { label: metric.name, num: true }],
			zoneRows.map(function (z) { return [placeLink(z.place, z.cells[0]), z.amount === null ? '' : num(z.amount)]; }),
			parseMetric(activity.metric).powerup ? 'Powerups by world: pick the powerup kind itself (World events) to see where.' : 'Nothing recorded.');
	}

	// The Where picker: everywhere, all properties, each property zone, each property, each other world
	function renderActivityPlaces() {
		var select = document.getElementById('activityPlace');
		var groups = [], byGroup = {};
		var places = activity.places.length ? activity.places.slice() : [{ place: '', name: 'Everywhere', group: '' }];
		if (activity.place && !places.some(function (p) { return p.place === activity.place; })) places.push({ place: activity.place, name: activity.placeName || activity.place, group: 'Other' });
		places.forEach(function (p) {
			var g = p.group || '';
			if (!byGroup[g]) { byGroup[g] = []; groups.push(g); }
			byGroup[g].push(p);
		});
		function option(p) {
			var count = (p.events || 0) + (p.stats || 0);
			return '<option value="' + esc(p.place) + '"' + (p.place === activity.place ? ' selected' : '') + '>' + esc(p.name) + (p.place && count ? ' (' + num(p.events || 0) + ' events)' : '') + '</option>';
		}
		select.innerHTML = groups.map(function (g) {
			return g ? '<optgroup label="' + esc(g) + '">' + byGroup[g].map(option).join('') + '</optgroup>' : byGroup[g].map(option).join('');
		}).join('');
		var current = places.filter(function (p) { return p.place === activity.place; })[0];
		var note = '';
		if (current && current.unknown) note = 'Recorded before properties were told apart, so which property is not known.';
		else if (activity.place === 'properties' || (current && current.property && current.place.indexOf(':') < 0)) note = 'Every property together (different builds on the same ground). Pick one property to see its own data.';
		if (current && current.property_id) note += (note ? ' ' : '') + '<a href="/properties/' + esc(current.property_id) + '">Property page</a>';
		if (current && current.owner_id) note += (note ? ' · ' : '') + 'Owner ' + charLink(current.owner_id, current.owner_name);
		if (current && current.place && current.place.indexOf(':') > 0 && current.property) note += (note ? ' · ' : '') + '<a href="#" data-map-place="' + esc(current.place) + '">World map</a>';
		document.getElementById('activityPlaceNote').innerHTML = note;
	}

	function loadActivity() {
		return Promise.all([api.get('/api/reports/activity' + query('&place=' + encodeURIComponent(activity.place))), api.get('/api/reports/places' + query(activity.place.indexOf('*:') === 0 ? '&place=' + encodeURIComponent(activity.place) : '')).catch(function () { return { places: [] }; })]).then(function (res) {
			activity.data = res[0];
			activity.places = res[1].places || [];
			renderActivityPlaces();
			renderActivity();
		});
	}

	function setActivityPlace(place) {
		activity.place = place || '';
		loadActivity();
	}

	document.getElementById('activityMetric').addEventListener('change', function (e) { activity.metric = e.target.value; renderActivity(); });
	document.getElementById('activityPlace').addEventListener('change', function (e) { setActivityPlace(e.target.value); });
	document.getElementById('activityZones').addEventListener('click', function (e) {
		var link = e.target.closest('[data-place]');
		if (!link) return;
		e.preventDefault();
		setActivityPlace(link.getAttribute('data-place'));
	});
	document.getElementById('activityPlaceNote').addEventListener('click', function (e) {
		var link = e.target.closest('[data-map-place]');
		if (!link) return;
		e.preventDefault();
		openMapPlace(link.getAttribute('data-map-place'));
	});
	document.getElementById('activityTotals').addEventListener('click', function (e) {
		var link = e.target.closest('[data-metric]');
		if (!link) return;
		e.preventDefault();
		activity.metric = link.getAttribute('data-metric');
		renderActivity();
	});
	document.getElementById('activityTabBtn').addEventListener('shown.bs.tab', function () {
		if (activity.loaded) return;
		activity.loaded = true;
		loadActivity();
	});

	// ---- Saved views ----
	var TAB_TARGETS = { coins: '#tabCoins', uscore: '#tabUScore', items: '#tabItems', activity: '#tabActivity', map: '#tabMap', transfers: '#tabTransfers', trace: '#tabTrace', dupes: '#tabDupes', flags: '#tabFlags' };
	var TAB_NAMES = { coins: 'Coins', uscore: 'U-Score', items: 'Items', activity: 'Activity', map: 'World Map', transfers: 'Trades & Mail', trace: 'Trace', dupes: 'Duplicates', flags: 'Flags' };
	var views = [], emailReady = false;

	function currentTab() {
		var active = document.querySelector('[data-bs-target^="#tab"].active');
		var target = active ? active.getAttribute('data-bs-target') : '#tabCoins';
		return Object.keys(TAB_TARGETS).filter(function (k) { return TAB_TARGETS[k] === target; })[0] || 'coins';
	}

	function describeView(v) {
		return TAB_NAMES[v.tab] + ', last ' + v.days + ' days' + (v.staff ? ', staff included' : '') + (v.lot ? ', ' + (v.lot_name || 'LOT ' + v.lot) : '');
	}

	function applyView(v) {
		var radio = document.getElementById('range' + v.days);
		if (radio) radio.checked = true;
		state.days = v.days;
		state.staff = !!v.staff;
		document.getElementById('includeStaff').checked = state.staff;
		state.lot = v.lot || 0;
		document.getElementById('itemLot').value = state.lot ? String(state.lot) : '';
		loadAll().then(updateCsvLinks);
		var button = document.querySelector('[data-bs-target="' + TAB_TARGETS[v.tab] + '"]');
		if (button) bootstrap.Tab.getOrCreateInstance(button).show();
		history.replaceState(null, '', '#view=' + encodeURIComponent(v.name));
	}

	function renderViews() {
		var menu = document.getElementById('viewsMenu');
		var items = views.map(function (v, i) {
			return '<li><a class="dropdown-item d-flex justify-content-between gap-3" href="#" data-view="' + i + '"><span>' + esc(v.name) +
				'<span class="d-block small text-body-secondary">' + esc(describeView(v)) + '</span></span>' +
				(v.email !== 'off' ? '<span class="badge text-bg-info align-self-center">' + esc(v.email) + '</span>' : '') + '</a></li>';
		});
		menu.innerHTML = (items.length ? items.join('') + '<li><hr class="dropdown-divider"></li>' : '<li><span class="dropdown-item-text small text-body-secondary">No saved views yet</span></li>') +
			'<li><a class="dropdown-item" href="#" id="saveViewBtn">Save this view&hellip;</a></li>' +
			(views.length ? '<li><a class="dropdown-item" href="#" id="manageViewsBtn">Manage views&hellip;</a></li>' : '');
		document.getElementById('viewRows').innerHTML = views.map(function (v, i) {
			return '<tr><td data-label="Name">' + esc(v.name) + '</td><td data-label="Shows" class="small">' + esc(describeView(v)) + '</td>' +
				'<td data-label="Email"><select class="form-select form-select-sm" data-email="' + i + '"' + (emailReady ? '' : ' disabled title="Confirm an email address on your account page first"') + '>' +
				['off', 'daily', 'weekly'].map(function (o) { return '<option value="' + o + '"' + (v.email === o ? ' selected' : '') + '>' + { off: 'No', daily: 'Every day', weekly: 'Every Monday' }[o] + '</option>'; }).join('') +
				'</select></td><td class="text-end text-nowrap">' + (emailReady ? '<button class="btn btn-sm btn-outline-secondary me-1" data-send="' + i + '">Email me now</button>' : '') +
				'<button class="btn btn-sm btn-outline-danger" data-remove="' + i + '">Delete</button></td></tr>';
		}).join('');
	}

	function loadViews() {
		return api.get('/api/reports/views').then(function (d) {
			if (!d.success) return;
			views = d.views; emailReady = d.emailReady;
			renderViews();
		});
	}

	function saveView(view) {
		return api.action('/api/reports/views', view).then(function (d) { toast(d.message, 'success'); views = d.views; renderViews(); });
	}

	var saveModal = new bootstrap.Modal(document.getElementById('saveViewModal'));
	var manageModal = new bootstrap.Modal(document.getElementById('manageViewsModal'));
	document.getElementById('viewsMenu').addEventListener('click', function (e) {
		var item = e.target.closest('[data-view]');
		if (item) { e.preventDefault(); applyView(views[Number(item.dataset.view)]); return; }
		if (e.target.closest('#saveViewBtn')) {
			e.preventDefault();
			var draft = { days: state.days, staff: state.staff, lot: state.lot, tab: currentTab(), lot_name: state.lotName };
			document.getElementById('saveViewSummary').textContent = 'Shows: ' + describeView(draft) + '. The range stays relative, so it always covers the latest days.';
			document.getElementById('viewName').value = '';
			document.getElementById('viewEmail').disabled = !emailReady;
			document.getElementById('viewEmailHelp').textContent = emailReady ? 'Reports cover the range up to yesterday and are sent around 07:00 UTC.'
				: 'To get it by email, confirm an email address on your account page (and the server needs email set up).';
			saveModal.show();
		}
		if (e.target.closest('#manageViewsBtn')) { e.preventDefault(); manageModal.show(); }
	});
	document.getElementById('saveViewForm').addEventListener('submit', function (e) {
		e.preventDefault();
		saveView({ name: document.getElementById('viewName').value.trim(), days: state.days, staff: state.staff, lot: state.lot, lot_name: state.lotName,
			tab: currentTab(), email: document.getElementById('viewEmail').value }).then(function () { saveModal.hide(); }).catch(function () {});
	});
	document.getElementById('viewRows').addEventListener('change', function (e) {
		var select = e.target.closest('[data-email]');
		if (select) saveView($.extend({}, views[Number(select.dataset.email)], { email: select.value })).catch(function () {});
	});
	document.getElementById('viewRows').addEventListener('click', function (e) {
		var send = e.target.closest('[data-send]'), remove = e.target.closest('[data-remove]');
		if (send) api.action('/api/reports/views/send', { name: views[Number(send.dataset.send)].name }).then(function (d) { toast(d.message, 'success'); }).catch(function () {});
		if (remove && confirm('Delete the view "' + views[Number(remove.dataset.remove)].name + '"?')) {
			api.action('/api/reports/views/delete', { name: views[Number(remove.dataset.remove)].name }).then(function (d) { toast(d.message, 'success'); views = d.views; renderViews(); }).catch(function () {});
		}
	});

	// ---- Transfers ----

	// Names come from the server's enums (Labels); only the colours are kept here
	var METHOD_COLOURS = { 1: 'info', 2: 'secondary', 3: 'success', 4: 'light' };
	function methodBadge(value) { return fmt.badge(Labels.name('transferMethods', value) || '?', METHOD_COLOURS[value] || 'secondary'); }
	var transfersTable = null;

	function traceLink(id) {
		if (!id || id === '0') return '<span class="text-body-secondary">-</span>';
		return '<a href="#" data-trace="' + esc(id) + '"><code>' + esc(id) + '</code></a>';
	}

	// Character names for the filter's suggestions, loaded the first time it's used
	var characterNamesLoaded = false;
	document.getElementById('transferCharacter').addEventListener('focus', function () {
		if (characterNamesLoaded) return;
		characterNamesLoaded = true;
		api.get('/api/characters/list').then(function (list) {
			var datalist = document.getElementById('characterNames');
			(Array.isArray(list) ? list : []).forEach(function (c) {
				var opt = document.createElement('option');
				opt.value = c.name;
				datalist.appendChild(opt);
			});
		});
	});

	function initTransfers() {
		if (transfersTable) return;
		transfersTable = serverTable('#transfersTable', '/api/reports/transfers', [
			{ data: 'time', orderable: false, render: function (d) { return esc(fmt.unix(d)); } },
			{ data: 'method', orderable: false, render: function (d) { return methodBadge(d); } },
			{ data: 'lot', orderable: false, render: function (d, t, row) { return d ? itemLink(d, row.name) : '<span class="text-body-secondary">Coins</span>'; } },
			{ data: 'count', orderable: false, className: 'text-end', render: function (d) { return d ? num(d) : ''; } },
			{ data: 'coins', orderable: false, className: 'text-end', render: function (d) { return d ? num(d) : ''; } },
			{ data: 'from_character', orderable: false, render: function (d, t, row) { return charLink(d, row.from_name); } },
			{ data: 'to_character', orderable: false, render: function (d, t, row) { return charLink(d, row.to_name); } },
			{ data: 'item_id', orderable: false, render: function (d) { return traceLink(d); } }
		], {
			dataTable: { searching: false, ordering: false },
			liveTable: 'economy',
			extra: function () {
				return {
					character: document.getElementById('transferCharacter').value.trim(),
					lot: parseInt(document.getElementById('transferLot').value, 10) || 0
				};
			}
		});
		$('#transfersTable').on('xhr.dt', function (e, settings, json) { if (json && json.error) toast(json.error, 'warning'); });
	}

	// ---- Trace ----

	// An item gets a new object ID at every trade, mail and inventory move; the server follows the chain from any of them
	var traceMerges = false;
	function shortId(id) { return '<a href="#" data-trace="' + esc(id) + '" title="Trace from ' + esc(id) + '"><code>' + esc(id) + '</code></a>'; }
	function whereText(l) { return l.where === 'mail' ? 'Mailbox (mail ' + l.mail_id + ')' : String(l.inventory || '').toLowerCase().replace(/_/g, ' '); }
	// A copy's character version (<lvl cv> in its save) and whether its next login gives it a new object ID: saves from
	// before item IDs were made unique get new IDs for every item at login
	function versionBadge(l) {
		if (l.where === 'mail' || l.character_version === undefined) return '';
		return l.new_id_at_login
			? ' <span class="badge text-bg-info" title="Saved at character version ' + esc(l.character_version) + ', before item IDs were made unique. The game gives its items new IDs the next time ' +
				esc(l.character_name || 'this character') + ' logs in.">old save v' + esc(l.character_version) + ': new ID at next login</span>'
			: ' <span class="badge text-bg-light border" title="Character version ' + esc(l.character_version) + ': already has unique item IDs">v' + esc(l.character_version) + '</span>';
	}
	// Whose login clears a shared ID (login_fix from the server): "Alice", "Alice or Bob", "all but one of ..."
	function loginNames(fix) {
		var names = (fix.characters || []).map(function (c) { return charLink(c.character_id, c.character_name); });
		var anyButOne = fix.logins_needed < names.length;
		if (names.length <= 1) return names.join('');
		if (names.length === 2) return names[0] + (anyButOne ? ' or ' : ' and ') + names[1];
		return (anyButOne ? 'all but one of ' : 'all of ') + names.join(', ');
	}
	// kind: 'collision' (different items: the login fixes it) or 'duplicate' (same item: the login only hides it)
	function loginFixCell(fix, kind) {
		if (!fix) return '';
		if (!fix.resolves) return kind === 'collision'
			? '<span class="badge text-bg-warning" title="The characters already have unique item IDs (or a copy is in mail), so logging in will not change it">Needs a look</span>'
			: '<span class="badge text-bg-danger">Needs a look</span>';
		return kind === 'collision'
			? '<span class="badge text-bg-success">Resolves on next login</span><div class="small text-body-secondary">at the next login of ' + loginNames(fix) + '</div>'
			: '<span class="badge text-bg-info" title="The login gives the old save\'s items new IDs: the shared ID goes away but every copy stays">IDs split on next login</span>' +
				'<div class="small text-body-secondary">at the next login of ' + loginNames(fix) + '; the copies stay, so check it first</div>';
	}
	var GAP_TEXT = {
		OWNER: 'Starts with someone the previous hop did not give it to: something in between was not recorded.',
		MAIL_NOT_SENT: 'Claimed from mail that was sent before sending was recorded (or those rows were pruned).',
		NO_NEW_ID: 'The new object ID was not recorded, so the trace cannot follow it further.'
	};

	function trace(id, merges) {
		id = String(id || '').trim();
		if (!/^\d+$/.test(id)) { toast('Enter a numeric object ID', 'warning'); return; }
		traceMerges = !!merges;
		document.getElementById('traceId').value = id;
		bootstrap.Tab.getOrCreateInstance(document.getElementById('traceTabBtn')).show();
		var out = document.getElementById('traceResult');
		out.innerHTML = '<p class="text-body-secondary">Searching...</p>';
		api.job('/api/reports/objects/' + id + (traceMerges ? '?merges=1' : ''), null, 'GET').catch(function (e) {
			out.innerHTML = '<div class="alert alert-danger">' + esc(e.message) + '</div>';
			return null;
		}).then(function (r) {
			if (!r) return;
			var history = r.history || [], ids = r.ids || [], latest = r.latest_ids || [];
			var lineHops = history.filter(function (h) { return h.role !== 'MERGE_IN'; });
			var title = r.lot ? esc(r.name || 'LOT ' + r.lot) + ' <span class="text-body-secondary small">LOT ' + esc(r.lot) + '</span>' : 'Unknown item';
			var status = !r.locations.length ? fmt.badge('Not found in any inventory or mail', 'secondary')
				: r.duplicated ? '<span class="badge text-bg-danger"><i class="bi bi-exclamation-octagon me-1" aria-hidden="true"></i>Duplicated: the same item is under one ID in more than one place</span>'
					: '<span class="badge text-bg-success"><i class="bi bi-check-circle me-1" aria-hidden="true"></i>Found</span>';
			if (r.collision) status += ' <span class="badge text-bg-warning" title="Old data gave the same object ID to different items. Nothing was copied.">' +
				'<i class="bi bi-intersect me-1" aria-hidden="true"></i>ID collision: a different item shares an ID</span>';
			if (r.resolves_on_login) status += ' <span class="badge text-bg-success" title="A character holding a copy has an old save; its next login gives its items new IDs, so the ID is no longer shared' +
				(r.duplicated ? '. The copies themselves stay.' : '.') + '"><i class="bi bi-box-arrow-in-right me-1" aria-hidden="true"></i>Resolves on next login</span>';
			var facts = [
				'<span class="text-body-secondary">Searched</span> ' + shortId(r.item_id),
				'<span class="text-body-secondary">First recorded ID</span> ' + shortId(r.first_id),
				'<span class="text-body-secondary">Latest</span> ' + (latest.length ? latest.map(shortId).join(', ') : '-'),
				num(lineHops.length) + ' hop' + (lineHops.length === 1 ? '' : 's') + ', ' + num(ids.length) + ' ID' + (ids.length === 1 ? '' : 's')
			];
			var html = '<div class="card mb-3"><div class="card-body"><h5 class="mb-1">' + title + '</h5><div class="mb-2">' + status + '</div>' +
				'<div class="small d-flex flex-wrap gap-3">' + facts.join('') + '</div>' +
				'<div class="form-check form-switch mt-2 small"><input class="form-check-input" type="checkbox" id="traceMerges"' + (traceMerges ? ' checked' : '') + '>' +
				'<label class="form-check-label" for="traceMerges">Also show the history of stacks it merged into</label></div></div></div>';

			// What the chain can't tell: shown above the timeline so it isn't missed
			var notes = [];
			if (r.truncated) notes.push('Stopped after ' + num(r.max_hops) + ' hops; trace from one of the later IDs to see the rest.');
			if (r.gaps) notes.push(num(r.gaps) + ' hop' + (r.gaps === 1 ? ' has' : 's have') + ' a gap before it (marked below): the chain was not recorded in full there.');
			if (history.length) {
				notes.push('Before ' + esc(fmt.unix(history[0].time)) + ' nothing is recorded for ' + shortId(r.first_id) +
					': it was looted, bought or made, or changed hands before trades and mail were recorded. Moves between a player\'s own inventories are recorded since the ledger started tracking them.');
			} else {
				notes.push('No trades, mail or inventory moves are recorded for this ID. It may never have changed hands, or it did before these records started.');
			}
			var gone = latest.filter(function (l) { var entry = ids.find(function (e) { return e.id === l; }); return entry && entry.checked && !entry.locations.length; });
			if (gone.length) notes.push('Latest ID ' + gone.map(shortId).join(', ') + ' is not in any saved inventory or unclaimed mail: sold, used, deleted, merged or moved before that was recorded, or in an online player\'s unsaved inventory.');
			html += '<div class="alert alert-secondary small">' + notes.map(function (n) { return '<div>' + n + '</div>'; }).join('') + '</div>';

			html += '<div class="card mb-3"><div class="card-header"><h5 class="mb-0">Where it is now</h5></div><div class="card-body table-responsive" id="traceLocations"></div></div>';
			html += '<div class="card mb-3"><div class="card-header"><h5 class="mb-0">Timeline</h5><div class="small text-body-secondary">Oldest first. Every hop gives the item a new object ID; click an ID to trace from it.</div></div>' +
				'<div class="card-body table-responsive" id="traceHistory"></div></div>';
			html += '<div class="card mb-3"><div class="card-header"><h5 class="mb-0">Its IDs</h5></div><div class="card-body table-responsive" id="traceIds"></div></div>';

			// Give it back under its latest ID when there is just one, so its history carries on
			var restoreId = latest.length === 1 ? latest[0] : r.item_id;
			if (DASH.can('items_restore') && r.lot) {
				// Suggest the player who last gave it away (usually the one asking for it back), or its last holder
				var last = lineHops.length ? lineHops[lineHops.length - 1] : null;
				var suggestion = last ? (r.locations.length ? last.from_name : last.to_name) || '' : (r.locations[0] ? r.locations[0].character_name : '');
				var restoreHere = ids.find(function (e) { return e.id === restoreId; });
				var restoreExists = restoreHere ? restoreHere.locations.length > 0 : r.locations.length > 0;
				html += '<div class="card"><div class="card-header"><h5 class="mb-0">Give it back</h5></div><div class="card-body">' +
					'<p class="small text-body-secondary">Mails the item (ID <code>' + esc(restoreId) + '</code>) to a player. ' + (restoreExists
						? 'It still exists, so they get a replacement with a new ID.' : 'It no longer exists anywhere, so it keeps that ID and its history carries on.') + '</p>' +
					'<form class="row g-2 align-items-end" id="restoreForm"><div class="col-sm-4"><label class="form-label small" for="restoreTo">To</label>' +
					'<input class="form-control form-control-sm" id="restoreTo" list="characterNames" value="' + esc(suggestion) + '" placeholder="Character name or ID" required></div>' +
					'<div class="col-sm-2"><label class="form-label small" for="restoreCount">Count</label><input type="number" min="1" max="999" class="form-control form-control-sm" id="restoreCount" value="' +
					esc((r.locations[0] && r.locations[0].count) || (last && last.count) || 1) + '"></div>' +
					'<div class="col-sm-4"><label class="form-label small" for="restoreNote">Note to the player</label><input class="form-control form-control-sm" id="restoreNote" maxlength="300" placeholder="Optional"></div>' +
					'<div class="col-sm-2"><button class="btn btn-sm btn-primary w-100">Mail it</button></div></form></div></div>';
			}
			out.innerHTML = html;
			document.getElementById('traceMerges').addEventListener('change', function (e) { trace(r.item_id, e.target.checked); });
			var restoreForm = document.getElementById('restoreForm');
			if (restoreForm) restoreForm.addEventListener('submit', function (e) {
				e.preventDefault();
				var to = document.getElementById('restoreTo').value.trim();
				if (!confirm('Mail ' + (r.name || 'this item') + ' to ' + to + '?')) return;
				api.job('/api/reports/objects/' + restoreId + '/restore', { to: to, lot: r.lot, count: parseInt(document.getElementById('restoreCount').value, 10) || 1,
					note: document.getElementById('restoreNote').value.trim() }).then(function (res) { toast(res.message, 'success'); }).catch(function () {});
			});

			table(document.getElementById('traceLocations'), [{ label: 'Item' }, { label: 'Character' }, { label: 'Where' }, { label: 'Count', num: true }, { label: 'Under ID' }],
				r.locations.map(function (l) {
					var other = r.lot && l.lot !== r.lot ? ' ' + fmt.badge('different item: ID collision', 'warning') : '';
					return [itemLink(l.lot, l.name) + other, charLink(l.character_id, l.character_name) + versionBadge(l), esc(whereText(l)), num(l.count),
						shortId(l.item_id) + (latest.indexOf(l.item_id) !== -1 ? ' ' + fmt.badge('latest', 'primary') : '')];
				}), 'Not under any of its IDs in a saved inventory or unclaimed mail.');

			var el = document.getElementById('traceHistory');
			if (!history.length) {
				el.innerHTML = '<p class="text-body-secondary mb-0">Nothing recorded.</p>';
			} else {
				el.innerHTML = '<table class="table table-sm align-middle mb-0"><thead><tr><th>Time</th><th>How</th><th>From</th><th>To</th><th>Zone</th>' +
					'<th class="text-end">Count</th><th class="text-end">Coins</th><th>Old ID → new ID</th></tr></thead><tbody>' +
					history.map(function (h) {
						var marks = [];
						if (h.role === 'BRANCH') marks.push('<span class="badge text-bg-light border" title="Another part of a stack this item came from">other part</span>');
						if (h.role === 'MERGE_IN') marks.push('<span class="badge text-bg-light border" title="Other items that joined this item\'s stack; their history is not followed">joined the stack</span>');
						if (h.merge) marks.push('<span class="badge text-bg-warning" title="Went into a stack the receiver already had; that stack\'s earlier history is ' +
							(traceMerges ? 'shown as other parts' : 'left out') + '">merged' + (h.merge_proven ? '*' : '') + '</span>');
						var gap = h.gap ? '<tr class="table-warning"><td colspan="8" class="small"><i class="bi bi-exclamation-triangle me-1" aria-hidden="true"></i>Gap: ' + esc(GAP_TEXT[h.gap] || h.gap) + '</td></tr>' : '';
						var muted = h.role === 'LINE' ? '' : ' class="text-body-secondary"';
						return gap + '<tr' + muted + '><td class="text-nowrap">' + esc(fmt.unix(h.time)) + '</td><td>' + methodBadge(h.method) + ' ' + marks.join(' ') + '</td>' +
							'<td>' + charLink(h.from_character, h.from_name) + '</td><td>' + charLink(h.to_character, h.to_name) + '</td>' +
							'<td>' + (h.zone ? fmt.zone(h.zone, h.zone_name) : '') + '</td><td class="text-end viz-num">' + (h.count ? num(h.count) : '') + '</td>' +
							'<td class="text-end viz-num">' + (h.coins ? num(h.coins) : '') + '</td><td class="text-nowrap">' + traceLink(h.item_id) + ' → ' + traceLink(h.new_item_id) + '</td></tr>';
					}).join('') + '</tbody></table>' +
					(history.some(function (h) { return h.merge_proven; }) ? '<div class="small text-body-secondary mt-1">* worked out from older rows, which did not record merges.</div>' : '');
			}

			table(document.getElementById('traceIds'), [{ label: 'Object ID' }, { label: 'Now' }],
				ids.map(function (e) {
					var now = !e.checked ? '<span class="text-body-secondary">not looked up</span>'
						: e.locations.length ? e.locations.map(function (l) { return itemLink(l.lot, l.name) + ' · ' + charLink(l.character_id, l.character_name) + versionBadge(l) + ' <span class="small text-body-secondary">' + esc(whereText(l)) + ' ×' + esc(l.count) + '</span>'; }).join('<br>') +
							(e.login_fix ? '<div class="mt-1">' + loginFixCell(e.login_fix, e.locations.every(function (l) { return l.lot === e.locations[0].lot; }) ? 'duplicate' : 'collision') + '</div>' : '')
							: '<span class="text-body-secondary">gone</span>';
					return [shortId(e.id) + (e.latest ? ' ' + fmt.badge('latest', 'primary') : '') + (e.id === r.first_id ? ' ' + fmt.badge('first', 'secondary') : ''), now];
				}));
		});
	}

	// ---- Duplicates ----

	function showScan(s) {
		var info = document.getElementById('scanInfo');
		if (!s.time) {
			info.textContent = 'No scan has run since the dashboard started. Scans read every character and unclaimed mail.';
			['dupeTable', 'collisionTable', 'heldTable'].forEach(function (id) { document.getElementById(id).innerHTML = '<p class="text-body-secondary mb-0">Run a scan to see results.</p>'; });
			return;
		}
		info.textContent = 'Last scan ' + fmt.unix(s.time) + ': ' + num(s.characters) + ' characters, ' + num(s.items) + ' items in ' + s.seconds.toFixed(1) + 's.';
		// Each copy with its own item, so a copy of a different item under the same ID shows as such
		function copyList(copies) {
			return copies.map(function (c) {
				return itemLink(c.lot, c.name) + ' · ' + charLink(c.character_id, c.character_name) + versionBadge(c) + ' <span class="small text-body-secondary">' +
					esc(whereText(c)) + ' ×' + esc(c.count) + '</span>';
			}).join('<br>');
		}
		table(document.getElementById('dupeTable'), [{ label: 'Item' }, { label: 'Object ID' }, { label: 'Copies' }, { label: 'On login' }],
			(s.duplicates || []).map(function (d) { return [itemLink(d.lot, d.name), traceLink(d.item_id), copyList(d.copies), loginFixCell(d.login_fix, 'duplicate')]; }), 'No duplicated items found.');
		var collisions = s.collisions || [];
		var resolving = collisions.filter(function (d) { return d.login_fix && d.login_fix.resolves; }).length;
		var summary = document.getElementById('collisionSummary');
		if (summary) summary.textContent = collisions.length ? num(resolving) + ' of ' + num(collisions.length) + ' resolve on the next login of a character with an old save; ' +
			num(collisions.length - resolving) + ' need a look.' : '';
		table(document.getElementById('collisionTable'), [{ label: 'Object ID' }, { label: 'Items under it' }, { label: 'On login' }],
			collisions.map(function (d) {
				return [traceLink(d.item_id) + (d.duplicated ? ' ' + fmt.badge('also duplicated', 'danger') : ''), copyList(d.copies), loginFixCell(d.login_fix, 'collision')];
			}), 'No object IDs shared by different items.');
		table(document.getElementById('heldTable'), [{ label: 'Item' }, { label: 'Held', num: true }, { label: 'Holders', num: true }],
			(s.heldByLot || []).map(function (h) { return [itemLink(h.lot, h.name), num(h.count), num(h.holders)]; }));
	}

	// ---- World map: where things happen, per recorded cell, over the zone's minimap or shaded terrain ----

	// Sequential blue ramp, dark (few) to light (many) so busy cells stand out on the dark surface
	var HEAT_RAMP = ['#184f95', '#1c5cab', '#256abf', '#2a78d6', '#3987e5', '#5598e7', '#6da7ec', '#86b6ef', '#9ec5f4', '#b7d3f6', '#cde2fb'];
	// Recorded cells are a few world units wide, a pixel or less when a whole zone is in view; they are merged into
	// squares at least this many pixels wide so every event shows. Zooming in splits them again.
	var HEAT_MIN_PX = 6;
	// Hotspots add up cells over this many cells a side
	var HOTSPOT_CELLS = 4;
	// Map event kinds come from the server (/api/reports/meta): {value, name, lots, quantity}
	// clone: on a property zone, one property (0: recorded before properties were told apart); null: every property together
	var map = { kind: 0, zone: 0, clone: null, lot: 0, backdrop: 'minimap', terrain: {}, tiles: {}, data: null, zones: [], players: [], showPlayers: document.getElementById('mapPlayers').checked, smooth: document.getElementById('mapSmooth').checked, view: null };

	function loadTerrain(zone) {
		if (map.terrain[zone] !== undefined) return Promise.resolve(map.terrain[zone]);
		return api.get('/api/reports/map/' + zone + '/terrain').then(function (t) {
			if (!t || t.success === false || !t.heights) return (map.terrain[zone] = null);
			var bytes = atob(t.heights);
			var heights = new Uint16Array(t.width * t.height);
			for (var i = 0; i < heights.length; i++) heights[i] = bytes.charCodeAt(i * 2) | (bytes.charCodeAt(i * 2 + 1) << 8);
			t.heights = heights;
			t.maxX = t.minX + (t.width - 1) * t.step;
			t.maxZ = t.minZ + (t.height - 1) * t.step;
			t.relief = shadeTerrain(t);
			return (map.terrain[zone] = t);
		}).catch(function () { return (map.terrain[zone] = null); });
	}

	// Hillshade, already turned 180 degrees like the game's minimap (x and z decrease to the right and down)
	function shadeTerrain(t) {
		var canvas = document.createElement('canvas');
		canvas.width = t.width;
		canvas.height = t.height;
		var ctx = canvas.getContext('2d');
		var img = ctx.createImageData(t.width, t.height);
		var range = (t.maxY - t.minY) || 1;
		var scale = range / 65534 / t.step * 3; // exaggerate relief a little
		function h(x, z) {
			x = Math.max(0, Math.min(t.width - 1, x));
			z = Math.max(0, Math.min(t.height - 1, z));
			var v = t.heights[z * t.width + x];
			return v === 65535 ? 0 : v;
		}
		for (var z = 0; z < t.height; z++) {
			for (var x = 0; x < t.width; x++) {
				var o = ((t.height - 1 - z) * t.width + (t.width - 1 - x)) * 4;
				var raw = t.heights[z * t.width + x];
				if (raw === 65535) { img.data[o + 3] = 0; continue; }
				var dx = (h(x + 1, z) - h(x - 1, z)) * scale;
				var dz = (h(x, z + 1) - h(x, z - 1)) * scale;
				var len = Math.sqrt(dx * dx + dz * dz + 4);
				// Lit from world +x/+z, which is the screen's upper left once the map is turned 180 degrees
				var light = Math.max(0, (2 - dx - dz) / len / 1.8);
				var elevation = raw / 65534;
				var v = 28 + 70 * light + 40 * elevation;
				img.data[o] = v * 0.92;
				img.data[o + 1] = v * 0.95;
				img.data[o + 2] = v;
				img.data[o + 3] = 255;
			}
		}
		ctx.putImageData(img, 0, 0);
		return canvas;
	}

	function loadMinimap(zone, tiles) {
		var key = zone + ':' + tiles;
		if (map.tiles[key]) return map.tiles[key];
		map.tiles[key] = Promise.all(Array.from({ length: tiles * tiles }, function (_, i) {
			return new Promise(function (resolve) {
				var img = new Image();
				img.onload = function () { resolve(img); };
				img.onerror = function () { resolve(null); };
				img.src = '/api/reports/map/' + zone + '/minimap/' + (i + 1);
			});
		})).then(function (images) {
			if (images.some(function (i) { return !i; })) return null;
			var canvas = document.createElement('canvas');
			canvas.width = canvas.height = tiles * 256;
			var ctx = canvas.getContext('2d');
			images.forEach(function (img, i) { ctx.drawImage(img, (i % tiles) * 256, Math.floor(i / tiles) * 256, 256, 256); });
			canvas.content = contentBox(ctx, canvas.width, canvas.height);
			return canvas;
		});
		return map.tiles[key];
	}

	// The part of the minimap with anything drawn on it (the playable area), as fractions of its size
	function contentBox(ctx, w, h) {
		var alpha = ctx.getImageData(0, 0, w, h).data;
		var box = { x0: w, y0: h, x1: -1, y1: -1 };
		for (var y = 0; y < h; y++) {
			for (var x = 0; x < w; x++) {
				if (alpha[(y * w + x) * 4 + 3] < 24) continue;
				if (x < box.x0) box.x0 = x;
				if (x > box.x1) box.x1 = x;
				if (y < box.y0) box.y0 = y;
				if (y > box.y1) box.y1 = y;
			}
		}
		if (box.x1 < 0) return null;
		return { x0: box.x0 / w, y0: box.y0 / h, x1: (box.x1 + 1) / w, y1: (box.y1 + 1) / h };
	}

	function heatColor(t) {
		return HEAT_RAMP[Math.max(0, Math.min(HEAT_RAMP.length - 1, Math.round(t * (HEAT_RAMP.length - 1))))];
	}

	// The area to show: the playable part of the zone (minimap content) plus every recorded cell, padded a little.
	// Without a minimap it is just the recorded cells, or the whole terrain when there are none.
	function viewBounds(data, terrain, minimap) {
		var size = data.cellSize;
		var b = { minX: Infinity, maxX: -Infinity, minZ: Infinity, maxZ: -Infinity };
		function include(minX, maxX, minZ, maxZ) {
			b.minX = Math.min(b.minX, minX); b.maxX = Math.max(b.maxX, maxX);
			b.minZ = Math.min(b.minZ, minZ); b.maxZ = Math.max(b.maxZ, maxZ);
		}
		data.cells.forEach(function (c) { include(c.x * size, (c.x + 1) * size, c.z * size, (c.z + 1) * size); });
		// A property's build area and models, so the map frames what is built there
		propertyAreas(data).forEach(function (a) { a.outline.forEach(function (p) { include(p[0], p[0], p[2], p[2]); }); });
		((data.property && data.property.models) || []).forEach(function (m) { include(m.x, m.x, m.z, m.z); });
		if (terrain && minimap && minimap.content) {
			// The minimap covers the terrain's bounds turned 180 degrees, like the relief
			var m = minimap.content, tX = terrain.maxX - terrain.minX, tZ = terrain.maxZ - terrain.minZ;
			include(terrain.maxX - m.x1 * tX, terrain.maxX - m.x0 * tX, terrain.maxZ - m.y1 * tZ, terrain.maxZ - m.y0 * tZ);
		}
		if (!isFinite(b.minX)) {
			if (!terrain) return null;
			return { minX: terrain.minX, maxX: terrain.maxX, minZ: terrain.minZ, maxZ: terrain.maxZ };
		}
		var pad = Math.max(size * 4, Math.max(b.maxX - b.minX, b.maxZ - b.minZ) * 0.06);
		b.minX -= pad; b.maxX += pad; b.minZ -= pad; b.maxZ += pad;
		if (terrain) {
			b.minX = Math.max(b.minX, terrain.minX); b.maxX = Math.min(b.maxX, terrain.maxX);
			b.minZ = Math.max(b.minZ, terrain.minZ); b.maxZ = Math.min(b.maxZ, terrain.maxZ);
		}
		return b;
	}

	// What the centre of a single cell keeps after blur(): 6/16 along each axis
	var BLUR_PEAK = (6 / 16) * (6 / 16);

	// Blur a grid in place with a [1 2 1]/4 kernel, twice along each axis (roughly a one-cell gaussian)
	function blur(grid, w, h) {
		var tmp = new Float32Array(grid.length);
		for (var pass = 0; pass < 2; pass++) {
			for (var y = 0; y < h; y++) {
				for (var x = 0; x < w; x++) {
					var i = y * w + x;
					tmp[i] = (grid[i] * 2 + (x > 0 ? grid[i - 1] : 0) + (x < w - 1 ? grid[i + 1] : 0)) / 4;
				}
			}
			for (var y2 = 0; y2 < h; y2++) {
				for (var x2 = 0; x2 < w; x2++) {
					var j = y2 * w + x2;
					grid[j] = (tmp[j] * 2 + (y2 > 0 ? tmp[j - w] : 0) + (y2 < h - 1 ? tmp[j + w] : 0)) / 4;
				}
			}
		}
	}

	function currentKind() {
		return mapKind(map.kind) || { value: map.kind, name: 'Events', lots: '', quantity: '' };
	}

	// Cells merged into squares of `bin` cells a side: {x, z, events, quantity} in units of the square
	function binCells(cells, bin) {
		if (bin === 1) return cells;
		var merged = {};
		cells.forEach(function (c) {
			var x = Math.floor(c.x / bin), z = Math.floor(c.z / bin), key = x + ',' + z;
			var m = merged[key] || (merged[key] = { x: x, z: z, events: 0, quantity: 0 });
			m.events += c.events;
			m.quantity += c.quantity;
		});
		return Object.keys(merged).map(function (k) { return merged[k]; });
	}

	// The smallest power of two of cells that is at least HEAT_MIN_PX wide on screen
	function binFor(cellSize, worldPerPixel) {
		var bin = 1;
		while (bin * cellSize / worldPerPixel < HEAT_MIN_PX && bin < 1024) bin *= 2;
		return bin;
	}

	var HEAT_RGB = HEAT_RAMP.map(function (hex) { return [parseInt(hex.substr(1, 2), 16), parseInt(hex.substr(3, 2), 16), parseInt(hex.substr(5, 2), 16)]; });

	// The heat layer as an image with one pixel per cell, already turned like the map (x and z decrease right/down)
	function heatImage(cells, smooth) {
		var x0 = Infinity, x1 = -Infinity, z0 = Infinity, z1 = -Infinity;
		cells.forEach(function (c) { x0 = Math.min(x0, c.x); x1 = Math.max(x1, c.x); z0 = Math.min(z0, c.z); z1 = Math.max(z1, c.z); });
		var pad = smooth ? 3 : 0;
		x0 -= pad; x1 += pad; z0 -= pad; z1 += pad;
		var w = x1 - x0 + 1, h = z1 - z0 + 1;
		var grid = new Float32Array(w * h);
		cells.forEach(function (c) { grid[(z1 - c.z) * w + (x1 - c.x)] += c.events; });
		if (smooth) {
			blur(grid, w, h);
			// Back to events: one event alone in a cell keeps BLUR_PEAK of itself at its centre
			for (var j = 0; j < grid.length; j++) grid[j] /= BLUR_PEAK;
		}
		var max = 0;
		for (var i = 0; i < grid.length; i++) max = Math.max(max, grid[i]);
		var logMax = Math.log(max + 1) || 1;
		var canvas = document.createElement('canvas');
		canvas.width = w;
		canvas.height = h;
		var ctx = canvas.getContext('2d');
		var img = ctx.createImageData(w, h);
		for (var k = 0; k < grid.length; k++) {
			if (grid[k] <= 0) continue;
			var t = Math.log(grid[k] + 1) / logMax;
			var rgb = HEAT_RGB[Math.min(HEAT_RGB.length - 1, Math.round(t * (HEAT_RGB.length - 1)))];
			img.data[k * 4] = rgb[0];
			img.data[k * 4 + 1] = rgb[1];
			img.data[k * 4 + 2] = rgb[2];
			// Squares show every recorded cell clearly; the smooth layer fades in from nothing so edges stay soft
			img.data[k * 4 + 3] = Math.round(255 * (smooth ? Math.min(0.9, t * 1.6) : 0.55 + 0.4 * t));
		}
		ctx.putImageData(img, 0, 0);
		// World extent of the image: its left edge is the high x side
		return { canvas: canvas, left: (x1 + 1), top: (z1 + 1), right: x0, bottom: z0 };
	}

	function drawMap(backdrop) {
		var data = map.data;
		var terrain = map.terrain[map.zone];
		var canvas = document.getElementById('mapCanvas');
		var box = document.getElementById('mapBox');
		var full = data ? viewBounds(data, terrain, map.minimap) : null;
		if (!full) {
			canvas.hidden = true;
			box.querySelector('.viz-empty') || box.insertAdjacentHTML('beforeend', '<p class="viz-empty text-body-secondary mb-0"></p>');
			box.querySelector('.viz-empty').textContent = data ? 'No ' + currentKind().name.toLowerCase() + ' recorded in this zone for this range.' : map.emptyText || 'Pick a zone.';
			document.getElementById('mapScale').hidden = true;
			return;
		}
		canvas.hidden = false;
		var empty = box.querySelector('.viz-empty');
		if (empty) empty.remove();
		map.full = full;
		var b = map.view || full;

		var aspect = (full.maxZ - full.minZ) / (full.maxX - full.minX);
		var width, height;
		if (getComputedStyle(canvas).position === 'absolute') {
			// The box fills the window (wide screens): the biggest map that fits in it
			width = box.clientWidth;
			height = Math.round(width * aspect);
			if (height > box.clientHeight) { height = Math.max(1, box.clientHeight); width = Math.round(height / aspect); }
		} else {
			width = Math.min(box.clientWidth, 900);
			height = Math.round(width * aspect);
			if (height > 760) { width = Math.round(width * 760 / height); height = 760; }
		}
		var spanX = b.maxX - b.minX, spanZ = b.maxZ - b.minZ;
		var dpr = window.devicePixelRatio || 1;
		canvas.width = width * dpr;
		canvas.height = height * dpr;
		canvas.style.width = width + 'px';
		canvas.style.height = height + 'px';
		map.boxSize = box.clientWidth + 'x' + box.clientHeight;
		var ctx = canvas.getContext('2d');
		ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
		ctx.fillStyle = getComputedStyle(box).getPropertyValue('--viz-surface') || '#212529';
		ctx.fillRect(0, 0, width, height);

		// Drawn like the game's minimap, turned 180 degrees: x and z both decrease to the right and down
		function sx(x) { return (b.maxX - x) / spanX * width; }
		function sy(z) { return (b.maxZ - z) / spanZ * height; }
		map.toWorld = function (px, py) { return { x: b.maxX - px / width * spanX, z: b.maxZ - py / height * spanZ }; };

		if (backdrop && terrain) {
			// The minimap and the relief both cover the terrain's bounds, turned like the map; cut out the part in view
			var image = backdrop.image;
			var tX = terrain.maxX - terrain.minX, tZ = terrain.maxZ - terrain.minZ;
			var srcX = (terrain.maxX - b.maxX) / tX * image.width, srcY = (terrain.maxZ - b.maxZ) / tZ * image.height;
			ctx.save();
			ctx.imageSmoothingEnabled = true;
			if (backdrop.minimap) ctx.globalAlpha = 0.6;
			ctx.drawImage(image, srcX, srcY, spanX / tX * image.width, spanZ / tZ * image.height, 0, 0, width, height);
			ctx.restore();
		}

		var bin = binFor(data.cellSize, spanX / width);
		var size = data.cellSize * bin;
		var cache = map.heatCache && map.heatCache.data === data && map.heatCache.bin === bin ? map.heatCache : { data: data, bin: bin, cells: binCells(data.cells, bin) };
		var cells = cache.cells;
		var max = 0;
		map.cellIndex = {};
		map.binSize = size;
		cells.forEach(function (c) { max = Math.max(max, c.events); map.cellIndex[c.x + ',' + c.z] = c; });
		map.hit = [];
		if (cells.length) {
			var heat = cache.smooth === map.smooth && cache.image ? cache.image : heatImage(cells, map.smooth);
			map.heatCache = { data: data, bin: bin, cells: cells, smooth: map.smooth, image: heat };
			ctx.save();
			// Squares stay crisp; the smooth layer is scaled with interpolation
			ctx.imageSmoothingEnabled = map.smooth;
			ctx.drawImage(heat.canvas, sx(heat.left * size), sy(heat.top * size), (heat.left - heat.right) * size / spanX * width, (heat.top - heat.bottom) * size / spanZ * height);
			ctx.restore();
		}

		// A property's build area (dashed outline) and placed models (small squares), so events line up with what is built
		propertyAreas(data).forEach(function (a) {
			if (a.outline.length < 2) return;
			ctx.save();
			ctx.beginPath();
			a.outline.forEach(function (p, i) { (i ? ctx.lineTo : ctx.moveTo).call(ctx, sx(p[0]), sy(p[2])); });
			ctx.closePath();
			ctx.setLineDash([6, 4]);
			ctx.lineWidth = 2;
			ctx.strokeStyle = '#ffffff';
			ctx.stroke();
			ctx.restore();
		});
		((data.property && data.property.models) || []).forEach(function (m) {
			var x = sx(m.x), y = sy(m.z);
			if (x < -4 || y < -4 || x > width + 4 || y > height + 4) return;
			ctx.fillStyle = '#4dabf7';
			ctx.strokeStyle = '#11141a';
			ctx.lineWidth = 1;
			ctx.fillRect(x - 3, y - 3, 6, 6);
			ctx.strokeRect(x - 3, y - 3, 6, 6);
			map.hit.push({ x: x - 3, y: y - 3, s: 6, model: m });
		});

		// Online players on top: white dots with a dark ring so they read on any backdrop
		var here = map.players.filter(function (p) { return p.zone === map.zone && (map.clone === null || (p.clone || 0) === map.clone); });
		document.getElementById('mapPlayerCount').textContent = map.players.length ? '(' + here.length + ' here, ' + map.players.length + ' online)' : '';
		if (map.showPlayers) {
			here.forEach(function (p) {
				var x = sx(p.x), y = sy(p.z);
				if (x < -6 || y < -6 || x > width + 6 || y > height + 6) return;
				ctx.beginPath();
				ctx.arc(x, y, 5, 0, Math.PI * 2);
				ctx.fillStyle = '#ffffff';
				ctx.fill();
				ctx.lineWidth = 2;
				ctx.strokeStyle = '#11141a';
				ctx.stroke();
				map.hit.push({ x: x - 6, y: y - 6, s: 12, player: p });
			});
		}

		document.getElementById('mapScale').hidden = !cells.length;
		document.getElementById('mapScaleMin').textContent = '1';
		document.getElementById('mapScaleMax').textContent = num(max) + ' (log scale)';
		document.getElementById('mapZoomReset').hidden = !map.view;
	}

	// Zoom around a point on the canvas; factor < 1 zooms in
	function zoomMap(factor, px, py) {
		if (!map.full || !map.toWorld) return;
		var full = map.full, b = map.view || full;
		var fullX = full.maxX - full.minX, fullZ = full.maxZ - full.minZ;
		var spanX = Math.min(fullX, Math.max(48, (b.maxX - b.minX) * factor));
		var spanZ = spanX * fullZ / fullX;
		var canvas = document.getElementById('mapCanvas');
		var fx = px / canvas.clientWidth, fz = py / canvas.clientHeight;
		var at = map.toWorld(px, py);
		var maxX = at.x + fx * spanX, maxZ = at.z + fz * spanZ;
		map.view = clampView({ minX: maxX - spanX, maxX: maxX, minZ: maxZ - spanZ, maxZ: maxZ });
		drawMap(map.backdropImage);
	}

	// Keep the view inside the full map, and drop it when it covers all of it
	function clampView(v) {
		var full = map.full;
		var spanX = v.maxX - v.minX, spanZ = v.maxZ - v.minZ;
		if (spanX >= full.maxX - full.minX - 0.01) return null;
		var shiftX = Math.max(0, full.minX - v.minX) - Math.max(0, v.maxX - full.maxX);
		var shiftZ = Math.max(0, full.minZ - v.minZ) - Math.max(0, v.maxZ - full.maxZ);
		return { minX: v.minX + shiftX, maxX: v.minX + shiftX + spanX, minZ: v.minZ + shiftZ, maxZ: v.minZ + shiftZ + spanZ };
	}

	function renderMap() {
		var terrain = map.terrain[map.zone];
		var useMinimap = map.backdrop === 'minimap' && terrain && terrain.minimapTiles;
		var backdrop = !terrain ? Promise.resolve(null)
			: useMinimap ? loadMinimap(map.zone, terrain.minimapTiles).then(function (c) { return c ? { minimap: true, image: c } : { image: terrain.relief }; })
				: Promise.resolve({ image: terrain.relief });
		// The minimap also decides the visible area, so load it for the terrain view too
		var minimap = terrain && terrain.minimapTiles ? loadMinimap(map.zone, terrain.minimapTiles) : Promise.resolve(null);
		document.getElementById('mapBackdropMinimap').disabled = !(terrain && terrain.minimapTiles);
		return Promise.all([backdrop, minimap]).then(function (res) {
			map.backdropImage = res[0];
			map.minimap = res[1];
			drawMap(res[0]);
		});
	}

	function mapSideTables() {
		var data = map.data;
		var kind = currentKind();
		var amount = kind.quantity || kind.name;
		document.getElementById('mapLotsTitle').textContent = kind.lots || kind.name;
		document.getElementById('mapLotAll').hidden = !map.lot;
		// Kinds without a meaningful LOT (coins) have one row, the total
		var lots = kind.lots ? data.lots || [] : (data.lots || []).slice(0, 1);
		table(document.getElementById('mapLots'), [{ label: kind.lots || 'In this zone' }, { label: amount, num: true }],
			lots.map(function (l) {
				var value = num(kind.quantity ? l.quantity : l.events);
				if (!kind.lots) return ['Total', value];
				var name = esc(l.name || (l.lot > 0 ? 'LOT ' + l.lot : 'Unknown')) + ' <span class="text-body-secondary small">' + esc(l.lot) + '</span>' +
					(l.type ? ' ' + fmt.badge(l.type, 'secondary') : '');
				return ['<a href="#" data-map-lot="' + esc(l.lot) + '"' + (l.lot === map.lot ? ' class="fw-semibold"' : '') + '>' + name + '</a>', value];
			}), 'No ' + kind.name.toLowerCase() + ' recorded.');

		var size = data.cellSize * HOTSPOT_CELLS;
		var hot = binCells(data.cells, HOTSPOT_CELLS).sort(function (a, b) { return b.events - a.events; }).slice(0, 12);
		table(document.getElementById('mapHotspots'), [{ label: 'Around (x, z)' }, { label: kind.name, num: true }],
			hot.map(function (c) {
				return ['<code>' + esc(Math.round((c.x + 0.5) * size)) + ', ' + esc(Math.round((c.z + 0.5) * size)) + '</code>', num(c.events)];
			}), 'Nothing here yet.');
	}

	// Nothing to show at all: say why instead of leaving an empty map and blank side cards
	function showNoMapData() {
		var kind = currentKind();
		map.data = null;
		map.emptyText = 'No ' + kind.name.toLowerCase() + ' recorded in this range.';
		document.getElementById('mapTitle').textContent = 'World map';
		document.getElementById('mapLotsTitle').textContent = kind.lots || kind.name;
		document.getElementById('mapLotAll').hidden = true;
		document.getElementById('mapLots').innerHTML = '<p class="text-body-secondary mb-0">' + esc(map.emptyText) + '</p>';
		document.getElementById('mapHotspots').innerHTML = '<p class="text-body-secondary mb-0">Nothing here yet.</p>';
		drawMap(null);
	}

	function loadMapData() {
		if (!map.zone) { showNoMapData(); return Promise.resolve(); }
		var pending = document.querySelector('#mapBox .viz-empty');
		if (pending) pending.textContent = 'Loading…';
		var q = query('&kind=' + map.kind + (map.lot ? '&lot=' + map.lot : '') + (map.clone !== null ? '&clone=' + map.clone : ''));
		return Promise.all([api.get('/api/reports/map/' + map.zone + q), loadTerrain(map.zone)]).then(function (res) {
			map.data = res[0];
			var lotName = '';
			(map.data.lots || []).forEach(function (l) { if (map.lot && l.lot === map.lot) lotName = l.name || 'LOT ' + l.lot; });
			var info = map.data.property && map.data.property.info;
			var where = info ? info.name : (map.data.property ? (map.data.name || 'Zone ' + map.zone) + ' · all properties' : map.data.name || 'Zone ' + map.zone);
			document.getElementById('mapTitle').textContent = where + (lotName ? ' · ' + lotName : '');
			showMapProperty();
			var note = 'Scroll to zoom, drag to move, hover for counts. Events are recorded in ' + map.data.cellSize + '×' + map.data.cellSize +
				' world unit squares and merged into larger ones until you zoom in.';
			if (!res[1]) note += ' No terrain for this zone (client_location unset or no .raw file), so only the recorded area is shown.';
			if (map.data.property && map.clone === null) note += ' Every property of this zone is drawn together: they are different builds on the same ground, so pick one property to see its own events.';
			if (map.data.property && map.clone !== null) note += ' The dashed line is where the owner may build; blue squares are placed models.';
			document.getElementById('mapNote').textContent = note;
			mapSideTables();
			return renderMap();
		});
	}

	function loadMapZones() {
		return api.get('/api/reports/map/zones' + query('&kind=' + map.kind)).then(function (res) {
			map.zones = res.zones || [];
			// Zones with players online are worth picking even before anything was recorded there
			map.players.forEach(function (p) {
				if (!map.zones.some(function (z) { return z.zone === p.zone; })) map.zones.push({ zone: p.zone, name: 'Zone ' + p.zone + ' (players online)', events: 0 });
			});
			var select = document.getElementById('mapZone');
			if (!map.zones.some(function (z) { return z.zone === map.zone; })) {
				// A property picked from a link keeps its zone even with nothing of this kind recorded there
				if (map.zone && map.clone !== null) map.zones.push({ zone: map.zone, name: 'Zone ' + map.zone, events: 0, property: true, properties: [] });
				else { map.zone = map.zones.length ? map.zones[0].zone : 0; map.clone = null; }
			}
			var all = res.allProperties || {};
			function option(z) {
				return '<option value="' + esc(z.zone) + '"' + (z.zone === map.zone ? ' selected' : '') + '>' + esc(z.name) + (z.property ? ' properties' : '') + ' (' + esc(num(z.events)) + ')</option>';
			}
			var worlds = map.zones.filter(function (z) { return !z.property; }), props = map.zones.filter(function (z) { return z.property; });
			select.innerHTML = map.zones.length ? worlds.map(option).join('') +
				(props.length ? '<optgroup label="' + esc('Properties: ' + num(all.events || 0) + ' on ' + num(all.properties || 0) + ' properties in all') + '">' + props.map(option).join('') + '</optgroup>' : '')
				: '<option value="0">No zones with data</option>';
			renderMapProperties();
			return loadMapData();
		});
	}

	// On a property zone: all its properties together, or one of them (each is its own instance of the zone)
	function renderMapProperties() {
		var select = document.getElementById('mapProperty');
		var zone = map.zones.filter(function (z) { return z.zone === map.zone; })[0];
		select.hidden = !(zone && zone.property);
		if (select.hidden) { map.clone = null; return; }
		var properties = (zone.properties || []).slice();
		if (map.clone !== null && !properties.some(function (p) { return p.clone === map.clone; })) {
			properties.push({ clone: map.clone, name: map.cloneName || 'Clone ' + map.clone, events: 0 });
		}
		select.innerHTML = '<option value="">All properties (' + esc(num(zone.events)) + ')</option>' + properties.map(function (p) {
			return '<option value="' + esc(p.clone) + '"' + (p.clone === map.clone ? ' selected' : '') + '>' + esc(p.name) + ' (' + esc(num(p.events)) + ')</option>';
		}).join('');
	}

	// The property card beside the map: who it belongs to, links, and the place's totals
	function showMapProperty() {
		var card = document.getElementById('mapPropertyCard');
		var property = map.data && map.data.property;
		card.hidden = !property;
		if (!property) return;
		var body = document.getElementById('mapPropertyBody');
		var info = property.info;
		if (!info) {
			var zone = map.zones.filter(function (z) { return z.zone === map.zone; })[0];
			document.getElementById('mapPropertyTitle').textContent = 'All properties here';
			body.innerHTML = '<p class="mb-2">' + esc(num(zone && zone.properties ? zone.properties.filter(function (p) { return !p.unknown; }).length : 0)) +
				' properties with ' + esc(currentKind().name.toLowerCase()) + ' in this range, drawn together. Pick one above to see its own map.</p>' +
				'<a href="#" data-activity-place="' + esc(map.zone) + '">Activity for these properties</a>';
			return;
		}
		document.getElementById('mapPropertyTitle').textContent = info.unknown ? 'Unknown property' : 'Property';
		if (info.unknown) {
			body.innerHTML = '<p class="mb-0">These events were recorded before the game told properties apart, so which property they happened on is not known. They are kept here rather than mixed into a property.</p>';
			return;
		}
		var rows = [];
		rows.push('<div class="fw-semibold">' + esc(info.property_name || info.name) + '</div>');
		if (info.owner_id) rows.push('<div>Owner ' + charLink(info.owner_id, info.owner_name) + '</div>');
		if (!info.property_id) rows.push('<div class="text-body-secondary">Not claimed: the owner visited this property without renting it.</div>');
		rows.push('<div class="text-body-secondary">' + esc(num((property.models || []).length)) + ' placed models · clone ' + esc(info.clone) + '</div>');
		var links = [];
		if (info.property_id) links.push('<a href="/properties/' + esc(info.property_id) + '">Property page</a>', '<a href="/properties/' + esc(info.property_id) + '/3d">3D view</a>');
		links.push('<a href="#" data-activity-place="' + esc(info.place) + '">Activity here</a>');
		rows.push('<div class="mt-1">' + links.join(' · ') + '</div>');
		rows.push('<div class="mt-2" id="mapPropertyTotals"><span class="text-body-secondary">Loading totals…</span></div>');
		body.innerHTML = rows.join('');
		api.get('/api/reports/place' + query('&place=' + encodeURIComponent(info.place))).then(function (t) {
			var el = document.getElementById('mapPropertyTotals');
			if (!el) return;
			var parts = [];
			(t.events || []).forEach(function (e) {
				var kind = mapKind(e.kind);
				parts.push([esc(e.name), num(kind && kind.quantity ? e.quantity : e.events) + (kind && kind.quantity ? ' ' + esc(kind.quantity.toLowerCase()) : '')]);
			});
			(t.powerups || []).forEach(function (p) { parts.push([esc((mapKind(p.kind) || {}).name || 'Powerups') + ': ' + esc(p.type), num(p.events)]); });
			(t.stats || []).forEach(function (st) { parts.push([esc(st.name), num(st.amount)]); });
			var html = parts.length ? '<table class="table table-sm mb-2"><tbody>' + parts.map(function (r) { return '<tr><td>' + r[0] + '</td><td class="text-end viz-num">' + r[1] + '</td></tr>'; }).join('') + '</tbody></table>'
				: '<p class="text-body-secondary mb-2">Nothing recorded here in this range.</p>';
			if (t.visitors) {
				html += '<div class="fw-semibold">Seen here</div>' + (t.visitors.length ? t.visitors.slice(0, 50).map(function (v) {
					return charLink(v.character_id, v.name) + ' <span class="text-body-secondary">' + esc(fmt.unix(v.last)) + '</span>';
				}).join('<br>') : '<span class="text-body-secondary">Nobody in the movement history (kept a few days).</span>');
			} else if (t.visitorsHidden) {
				html += '<div class="text-body-secondary">Who was here needs the movement history permission.</div>';
			}
			el.innerHTML = html;
		}).catch(function () {});
	}

	// Open the World Map on one place: '<zone>' or '<zone>:<clone>'
	function openMapPlace(place) {
		var parts = String(place).split(':');
		map.zone = parseInt(parts[0], 10) || 0;
		map.clone = parts.length > 1 ? parseInt(parts[1], 10) || 0 : null;
		map.lot = 0;
		map.view = null;
		// Once the map has loaded, showing the tab only redraws, so load the new place here
		if (mapLoaded) loadMapZones();
		bootstrap.Tab.getOrCreateInstance(document.getElementById('mapTabBtn')).show();
	}

	document.getElementById('mapPropertyBody').addEventListener('click', function (e) {
		var link = e.target.closest('[data-activity-place]');
		if (!link) return;
		e.preventDefault();
		activity.place = link.getAttribute('data-activity-place');
		activity.loaded = true;
		loadActivity();
		bootstrap.Tab.getOrCreateInstance(document.getElementById('activityTabBtn')).show();
	});

	// A property zone's build areas from the map data, as [{outline: [[x, y, z]]}]
	function propertyAreas(data) {
		var areas = data && data.property && data.property.areas;
		return Array.isArray(areas) ? areas.filter(function (a) { return a && Array.isArray(a.outline); }) : [];
	}

	var mapCanvas = document.getElementById('mapCanvas');
	var drag = null;
	mapCanvas.addEventListener('mousemove', function (evt) {
		var r = mapCanvas.getBoundingClientRect();
		var x = evt.clientX - r.left, y = evt.clientY - r.top;
		if (drag) {
			var dx = (x - drag.x) / r.width * (drag.view.maxX - drag.view.minX), dz = (y - drag.y) / r.height * (drag.view.maxZ - drag.view.minZ);
			// Screen right is lower x, so dragging right shows higher x
			map.view = clampView({ minX: drag.view.minX + dx, maxX: drag.view.maxX + dx, minZ: drag.view.minZ + dz, maxZ: drag.view.maxZ + dz });
			drawMap(map.backdropImage);
			return;
		}
		var hit = null;
		(map.hit || []).forEach(function (h) { if (x >= h.x - 2 && x <= h.x + h.s + 2 && y >= h.y - 2 && y <= h.y + h.s + 2) hit = h; });
		if (hit && hit.model) {
			showTip(evt, '<div class="fw-semibold">' + esc(hit.model.name || 'LOT ' + hit.model.lot) + '</div><div class="small text-body-secondary">Placed model · x ' +
				esc(Math.round(hit.model.x)) + ', z ' + esc(Math.round(hit.model.z)) + '</div>');
			return;
		}
		if (hit && hit.player) {
			showTip(evt, '<div class="fw-semibold">' + esc(hit.player.name) + '</div><div class="small text-body-secondary">Instance ' + esc(hit.player.instance) +
				(hit.player.clone ? ', clone ' + esc(hit.player.clone) : '') + ' · x ' + esc(Math.round(hit.player.x)) + ', z ' + esc(Math.round(hit.player.z)) + '</div>');
			return;
		}
		if (!map.toWorld || !map.data) { hideTip(); return; }
		var size = map.binSize, at = map.toWorld(x, y);
		var c = map.cellIndex && map.cellIndex[Math.floor(at.x / size) + ',' + Math.floor(at.z / size)];
		if (!c) { hideTip(); return; }
		var kind = currentKind();
		var what = kind.name + ': ' + num(c.events) + (kind.quantity ? ' (' + num(c.quantity) + ' ' + kind.quantity.toLowerCase() + ')' : '');
		showTip(evt, '<div class="fw-semibold">' + esc(what) + '</div><div class="small text-body-secondary">x ' + esc(c.x * size) + ' to ' + esc((c.x + 1) * size) +
			', z ' + esc(c.z * size) + ' to ' + esc((c.z + 1) * size) + '</div>');
	});
	// The wheel zooms the map and never scrolls the page, also over the space around a map narrower than its box
	document.getElementById('mapBox').addEventListener('wheel', function (evt) {
		if (mapCanvas.hidden) return;
		evt.preventDefault();
		var r = mapCanvas.getBoundingClientRect();
		zoomMap(evt.deltaY < 0 ? 0.8 : 1.25, Math.min(Math.max(evt.clientX - r.left, 0), r.width), Math.min(Math.max(evt.clientY - r.top, 0), r.height));
	}, { passive: false });
	// Redraw when the map's box changes size (the window, or the space it fills)
	if (window.ResizeObserver) new ResizeObserver(function () {
		var box = document.getElementById('mapBox');
		if (mapLoaded && map.data && box.clientWidth && map.boxSize !== box.clientWidth + 'x' + box.clientHeight) drawMap(map.backdropImage);
	}).observe(document.getElementById('mapBox'));
	mapCanvas.addEventListener('mousedown', function (evt) {
		if (!map.view) return;
		var r = mapCanvas.getBoundingClientRect();
		drag = { x: evt.clientX - r.left, y: evt.clientY - r.top, view: map.view };
		hideTip();
	});
	window.addEventListener('mouseup', function () { drag = null; });
	mapCanvas.addEventListener('dblclick', function (evt) {
		var r = mapCanvas.getBoundingClientRect();
		zoomMap(0.5, evt.clientX - r.left, evt.clientY - r.top);
	});
	document.getElementById('mapZoomReset').addEventListener('click', function () { map.view = null; drawMap(map.backdropImage); });
	document.getElementById('mapSmooth').addEventListener('change', function (e) { map.smooth = e.target.checked; drawMap(map.backdropImage); });
	mapCanvas.addEventListener('mouseleave', hideTip);

	document.getElementById('mapKind').addEventListener('change', function (e) { map.kind = parseInt(e.target.value, 10) || 0; map.lot = 0; map.view = null; loadMapZones(); });
	document.querySelectorAll('input[name="mapBackdrop"]').forEach(function (r) {
		r.addEventListener('change', function () { map.backdrop = r.value; renderMap(); });
	});
	document.getElementById('mapZone').addEventListener('change', function (e) { map.zone = parseInt(e.target.value, 10) || 0; map.clone = null; map.lot = 0; map.view = null; renderMapProperties(); loadMapData(); });
	document.getElementById('mapProperty').addEventListener('change', function (e) { map.clone = e.target.value === '' ? null : parseInt(e.target.value, 10) || 0; map.lot = 0; map.view = null; loadMapData(); });
	document.getElementById('mapLotAll').addEventListener('click', function () { map.lot = 0; loadMapData(); });
	document.getElementById('mapLots').addEventListener('click', function (e) {
		var link = e.target.closest('[data-map-lot]');
		if (!link) return;
		e.preventDefault();
		e.stopPropagation();
		var lot = parseInt(link.getAttribute('data-map-lot'), 10);
		map.lot = map.lot === lot ? 0 : lot;
		loadMapData();
	});
	document.getElementById('mapPlayers').addEventListener('change', function (e) { map.showPlayers = e.target.checked; drawMap(map.backdropImage); });

	var redrawPlayers = Live.throttle(function () { if (mapLoaded && map.data) drawMap(map.backdropImage); }, 1000);
	function setPlayers(players) {
		map.players = players || [];
		redrawPlayers();
	}

	var mapLoaded = false;
	document.getElementById('mapTabBtn').addEventListener('shown.bs.tab', function () {
		if (!mapLoaded) {
			mapLoaded = true;
			if (DASH.can('players_view')) {
				// Players first: zones with someone online are offered even when nothing was recorded there yet
				api.get('/api/live/players').then(function (r) { setPlayers(r.players || []); }).catch(function () {}).then(loadMapZones);
				if (window.Live) Live.onTopic('player_positions', function (e) { setPlayers(e.players); });
			} else loadMapZones();
		} else drawMap(map.backdropImage);
	});

	// ---- Flags ----

	var FLAG_KIND_COLOURS = { 1: 'warning', 2: 'info', 3: 'danger', 4: 'warning', 5: 'danger' };
	var FLAG_STATUS_COLOURS = { 0: 'danger', 1: 'secondary', 2: 'success' };
	function flagKindBadge(value) { return fmt.badge(Labels.name('flagKinds', value) || '?', FLAG_KIND_COLOURS[value] || 'secondary'); }
	function flagStatusBadge(value) { return fmt.badge(Labels.name('flagStatus', value) || '?', FLAG_STATUS_COLOURS[value] || 'secondary'); }
	var flagsTable = null, reviewing = null;

	function flagStatus() {
		var checked = document.querySelector('input[name="flagStatus"]:checked');
		return checked ? parseInt(checked.value, 10) : 0;
	}

	function initFlags() {
		if (flagsTable) return;
		flagsTable = serverTable('#flagsTable', '/api/reports/flags', [
			{ data: 'day', orderable: false, render: function (d, t, row) { return esc(d ? dayLabel(d, true) : fmt.unix(row.created_at)); } },
			{ data: 'kind', orderable: false, render: function (d) { return flagKindBadge(d); } },
			{ data: 'character_id', orderable: false, render: function (d, t, row) {
				var parts = [];
				if (d && d !== '0') parts.push(charLink(d, row.character_name));
				if (row.lot) parts.push(itemLink(row.lot, row.name));
				if (row.item_id && row.item_id !== '0') parts.push(traceLink(row.item_id));
				return parts.join('<br>');
			} },
			{ data: 'details', orderable: false, render: function (d, t, row) {
				return esc(d) + (row.note ? '<div class="small text-body-secondary mt-1">' + esc(row.reviewer_name) + ': ' + esc(row.note) + '</div>' : '');
			} },
			{ data: 'status', orderable: false, render: function (d) { return flagStatusBadge(d); } },
			{ data: 'id', orderable: false, render: function (d, t, row) {
				if (!DASH.can('reports_review_flags')) return '';
				return '<div class="d-flex justify-content-end gap-1"><button class="btn btn-sm btn-outline-primary" data-review="' + esc(d) + '" data-details="' + esc(row.details) + '">' + (row.status === 0 ? 'Review' : 'Change') + '</button>' +
					(row.status === 0 ? AiSuggest.button('economy_flag', d) : '') + '</div>';
			} }
		], { dataTable: { searching: false, ordering: false }, liveTable: 'economy_flags', extra: function () { return { status: flagStatus() }; } });
	}

	var flagModal = new bootstrap.Modal(document.getElementById('flagModal'));
	document.getElementById('flagsTable').addEventListener('click', function (e) {
		var button = e.target.closest('[data-review]');
		if (!button) return;
		reviewing = button.getAttribute('data-review');
		document.getElementById('flagSummary').textContent = button.getAttribute('data-details');
		document.getElementById('flagNote').value = '';
		flagModal.show();
	});
	document.querySelectorAll('[data-flag-status]').forEach(function (button) {
		button.addEventListener('click', function () {
			api.action('/api/reports/flags/' + reviewing + '/review', { status: button.getAttribute('data-flag-status'), note: document.getElementById('flagNote').value.trim() })
				.then(function () { flagModal.hide(); flagsTable.ajax.reload(null, false); }).catch(function () {});
		});
	});
	document.querySelectorAll('input[name="flagStatus"]').forEach(function (r) {
		r.addEventListener('change', function () { if (flagsTable) flagsTable.ajax.reload(); });
	});
	var runChecks = document.getElementById('runChecks');
	if (runChecks) runChecks.addEventListener('click', function () {
		runChecks.disabled = true;
		toast('Running the checks in the background...', 'info');
		api.job('/api/reports/flags/run', { duplicates: true }).then(function (r) {
			toast(r.message, 'info');
			if (flagsTable) flagsTable.ajax.reload();
		}).catch(function () {}).finally(function () { runChecks.disabled = false; });
	});
	document.getElementById('flagsTabBtn').addEventListener('shown.bs.tab', initFlags);

	// ---- CSV downloads: links follow the current range and filters ----

	// Same rules as the server's CSV: quote when needed, and keep spreadsheets from running player-written text
	function csvCell(value) {
		var text = value === null || value === undefined ? '' : String(value);
		if (/^[=+\-@\t\r]/.test(text) && !/^-[0-9.]+$/.test(text)) text = "'" + text;
		return /[",\r\n]/.test(text) ? '"' + text.replace(/"/g, '""') + '"' : text;
	}

	function toCsv(rows, columns) {
		return [columns.map(function (c) { return csvCell(c[1]); }).join(',')].concat(rows.map(function (row) {
			return columns.map(function (c) { return csvCell(row[c[0]]); }).join(',');
		})).join('\r\n') + '\r\n';
	}

	function updateCsvLinks() {
		var base = '/api/reports/';
		var transferQuery = [];
		var character = document.getElementById('transferCharacter').value.trim();
		var lot = document.getElementById('transferLot').value.trim();
		if (character) transferQuery.push('character=' + encodeURIComponent(character));
		if (/^\d+$/.test(lot)) transferQuery.push('lot=' + lot);
		var links = {
			currency: base + 'currency/csv' + query(),
			uscore: base + 'uscore/csv' + query(),
			top_earners: base + 'top_earners/csv' + query('&limit=1000'),
			items: base + 'items/csv' + query(state.lot ? '&lot=' + state.lot : ''),
			item_side: state.lot ? (holders.lot === state.lot && holders.csvUrl ? holders.csvUrl : '#') : base + 'top_items/csv' + query('&limit=1000'),
			transfers: base + 'transfers/csv' + (transferQuery.length ? '?' + transferQuery.join('&') : ''),
			flags: base + 'flags/csv?status=' + flagStatus(),
			duplicates: base + 'duplicates/csv'
		};
		document.querySelectorAll('[data-csv]').forEach(function (a) {
			var href = links[a.getAttribute('data-csv')];
			if (href) a.setAttribute('href', href);
		});
	}
	document.getElementById('transferCharacter').addEventListener('input', updateCsvLinks);
	document.getElementById('transferLot').addEventListener('input', updateCsvLinks);
	document.querySelectorAll('input[name="flagStatus"]').forEach(function (r) { r.addEventListener('change', updateCsvLinks); });

	// ---- Wiring ----

	function loadAll() {
		var loads = [loadCoins(), loadUScore(), loadItems()];
		if (activity.loaded) loads.push(loadActivity());
		if (mapLoaded) loads.push(loadMapZones());
		return Promise.all(loads).then(function () {
			updateCsvLinks();
			document.getElementById('updatedAt').textContent = 'Updated ' + new Date().toLocaleTimeString();
			if (transfersTable) transfersTable.ajax.reload(null, false);
		}).catch(function (e) { toast(e.message || 'Failed to load reports', 'danger'); });
	}

	document.addEventListener('click', function (e) {
		var lotLink = e.target.closest('[data-lot]');
		if (lotLink) {
			e.preventDefault();
			setLot(parseInt(lotLink.getAttribute('data-lot'), 10));
			bootstrap.Tab.getOrCreateInstance(document.querySelector('[data-bs-target="#tabItems"]')).show();
			return;
		}
		if (e.target.closest('[data-include-staff]')) {
			e.preventDefault();
			document.getElementById('includeStaff').checked = true;
			state.staff = true;
			loadAll();
			return;
		}
		var traceTarget = e.target.closest('[data-trace]');
		if (traceTarget) { e.preventDefault(); trace(traceTarget.getAttribute('data-trace')); }
	});

	document.querySelectorAll('input[name="range"]').forEach(function (r) {
		r.addEventListener('change', function () { state.days = parseInt(r.value, 10); loadAll(); });
	});
	document.getElementById('includeStaff').addEventListener('change', function (e) { state.staff = e.target.checked; loadAll(); });

	var suggestTimer = null;
	var itemInput = document.getElementById('itemLot');
	itemInput.addEventListener('input', function () {
		clearTimeout(suggestTimer);
		var q = itemInput.value.trim();
		if (q.length < 2 || /^\d+$/.test(q)) return;
		suggestTimer = setTimeout(function () {
			api.get('/api/items/search?q=' + encodeURIComponent(q)).then(function (items) {
				document.getElementById('itemSuggestions').innerHTML = (Array.isArray(items) ? items : []).map(function (i) {
					return '<option value="' + esc(i.lot) + '">' + esc(i.name) + '</option>';
				}).join('');
			});
		}, 250);
	});
	document.getElementById('itemFilter').addEventListener('submit', function (e) {
		e.preventDefault();
		var lot = parseInt(itemInput.value, 10);
		if (itemInput.value.trim() && !(lot > 0)) { toast('Pick an item from the list or enter its LOT', 'warning'); return; }
		setLot(lot || 0);
	});
	document.getElementById('itemClear').addEventListener('click', function () { setLot(0); });

	document.getElementById('transferFilter').addEventListener('submit', function (e) {
		e.preventDefault();
		if (transfersTable) transfersTable.ajax.reload();
	});
	document.querySelector('[data-bs-target="#tabTransfers"]').addEventListener('shown.bs.tab', initTransfers);
	document.querySelector('[data-bs-target="#tabDupes"]').addEventListener('shown.bs.tab', function () {
		api.get('/api/reports/duplicates').then(showScan);
	});
	document.getElementById('traceForm').addEventListener('submit', function (e) { e.preventDefault(); trace(document.getElementById('traceId').value); });

	document.getElementById('scanBtn').addEventListener('click', function () {
		var btn = this;
		btn.disabled = true;
		document.getElementById('scanInfo').textContent = 'Scanning every character in the background. You can keep using the dashboard.';
		api.job('/api/reports/duplicates/scan').then(showScan).catch(function () {
			api.get('/api/reports/duplicates').then(showScan);
		}).finally(function () { btn.disabled = false; });
	});

	// Charts in hidden tabs have no width; draw them when their tab opens, and again on resize
	document.querySelectorAll('button[data-bs-toggle="tab"]').forEach(function (b) {
		b.addEventListener('shown.bs.tab', function () { Object.keys(charts).forEach(function (k) { charts[k](); }); });
	});
	var resizeTimer = null;
	window.addEventListener('resize', function () {
		clearTimeout(resizeTimer);
		resizeTimer = setTimeout(function () {
			Object.keys(charts).forEach(function (k) { charts[k](); });
			if (mapLoaded) drawMap(map.backdropImage);
		}, 150);
	});

	api.get('/api/reports/meta').then(function (meta) {
		state.today = meta.today;
		state.sources = meta.sources || {};
		state.mapKinds = meta.mapKinds || [];
		state.stats = meta.stats || [];
		state.powerupKinds = meta.powerupKinds || {};
		map.kind = state.mapKinds.length ? state.mapKinds[0].value : 0;
		document.getElementById('mapKind').innerHTML = state.mapKinds.map(function (k) {
			return '<option value="' + esc(k.value) + '">' + esc(k.name) + '</option>';
		}).join('');
		var params = new URLSearchParams(window.location.search);
		if (params.get('lot')) state.lot = parseInt(params.get('lot'), 10) || 0;
		if (state.lot) itemInput.value = String(state.lot);
		loadAll().then(function () {
			if (params.get('object')) trace(params.get('object'));
		});
		var tab = { '#activity': 'activityTabBtn', '#map': 'mapTabBtn', '#trace': 'traceTabBtn', '#flags': 'flagsTabBtn' }[window.location.hash];
		if (tab) bootstrap.Tab.getOrCreateInstance(document.getElementById(tab)).show();
		// Links from property and character pages: ?place=<zone>:<clone> opens that property's map, other places the Activity tab
		var placeParam = params.get('place');
		if (placeParam) {
			activity.place = placeParam;
			if (/^\d+:\d+$/.test(placeParam)) openMapPlace(placeParam);
			else bootstrap.Tab.getOrCreateInstance(document.getElementById('activityTabBtn')).show();
		}
		// Links from report emails: #view=<name>
		loadViews().then(function () {
			var match = window.location.hash.match(/^#view=(.+)$/);
			var view = match && views.filter(function (v) { return v.name === decodeURIComponent(match[1]); })[0];
			if (view) applyView(view);
		});
		// World servers report each ledger write (every few seconds while something happens) over the live socket
		var stale = false;
		if (window.Live) Live.on('economy', Live.throttle(function () {
			if (document.hidden) stale = true;
			else loadAll();
		}, REFRESH_MS));
		document.addEventListener('visibilitychange', function () {
			if (!document.hidden && stale) { stale = false; loadAll(); }
		});
	});
})();
