/**
 * The 3D world view's bookkeeping, without three.js so it can be tested with node: smoothing live positions (worlds
 * report about once a second), finding where a player was at a moment of a replay, the heat map timelapse frames, and
 * following a player from one world to the next (live and in capture playback) with the timeline's world markers.
 */

/**
 * A live player's recent positions, drawn a little in the past (`delay` seconds) so there is always a report on
 * each side to interpolate between; past the newest report it holds still.
 */
export class Track {
	constructor(limit = 8) {
		this.samples = []; // [{t, x, y, z}], oldest first
		this.limit = limit;
	}

	push(t, x, y, z) {
		const last = this.samples[this.samples.length - 1];
		if (last && t <= last.t) {
			// Two reports in the same instant: keep the newer position
			last.x = x; last.y = y; last.z = z;
			return;
		}
		this.samples.push({ t, x, y, z });
		if (this.samples.length > this.limit) this.samples.shift();
	}

	get last() { return this.samples[this.samples.length - 1] || null; }

	/** Position at time t: interpolated between the reports around it; out holds the result. */
	sample(t, out = {}) {
		const s = this.samples;
		if (!s.length) return null;
		if (t <= s[0].t) return Object.assign(out, { x: s[0].x, y: s[0].y, z: s[0].z });
		for (let i = s.length - 1; i > 0; i--) {
			const a = s[i - 1], b = s[i];
			if (t >= a.t && t <= b.t) {
				const f = (t - a.t) / (b.t - a.t);
				return Object.assign(out, { x: a.x + (b.x - a.x) * f, y: a.y + (b.y - a.y) * f, z: a.z + (b.z - a.z) * f });
			}
		}
		const last = s[s.length - 1];
		return Object.assign(out, { x: last.x, y: last.y, z: last.z });
	}
}

/**
 * Where a replayed player was at time t, from their samples [t, x, y, z, t, x, y, z, ...] (t ascending). Between two
 * samples more than `gap` seconds apart they were elsewhere (another instance, offline), so null; the same outside
 * their first and last sample, give or take `hold` seconds.
 */
export function replayPosition(samples, t, gap, hold, out = {}) {
	const n = samples.length / 4;
	if (!n) return null;
	if (t < samples[0] - hold || t > samples[(n - 1) * 4] + hold) return null;
	// Last sample at or before t
	let lo = 0, hi = n - 1;
	if (t < samples[0]) return Object.assign(out, { x: samples[1], y: samples[2], z: samples[3] });
	while (lo < hi) {
		const mid = (lo + hi + 1) >> 1;
		if (samples[mid * 4] <= t) lo = mid; else hi = mid - 1;
	}
	const a = lo * 4;
	if (lo === n - 1) return Object.assign(out, { x: samples[a + 1], y: samples[a + 2], z: samples[a + 3] });
	const b = a + 4;
	const span = samples[b] - samples[a];
	if (span > gap) {
		// Stood still or left: stay where they were for `hold`, then gone until the next sample
		return t - samples[a] <= hold ? Object.assign(out, { x: samples[a + 1], y: samples[a + 2], z: samples[a + 3] }) : null;
	}
	const f = span > 0 ? (t - samples[a]) / span : 0;
	return Object.assign(out, {
		x: samples[a + 1] + (samples[b + 1] - samples[a + 1]) * f,
		y: samples[a + 2] + (samples[b + 2] - samples[a + 2]) * f,
		z: samples[a + 3] + (samples[b + 3] - samples[a + 3]) * f
	});
}

/**
 * Trail segments for a replay: for every two consecutive samples of a player no more than `gap` apart, the segment's
 * two ends as positions and times. Returns {positions: Float32Array (x,y,z per end), times: Float32Array, owners:
 * Uint32Array (player index per end)}.
 */
export function trailSegments(players, gap) {
	let count = 0;
	for (const p of players) {
		const s = p.samples;
		for (let i = 4; i < s.length; i += 4) if (s[i] - s[i - 4] <= gap) count++;
	}
	const positions = new Float32Array(count * 6), times = new Float32Array(count * 2), owners = new Uint32Array(count * 2);
	let k = 0;
	players.forEach((p, index) => {
		const s = p.samples;
		for (let i = 4; i < s.length; i += 4) {
			if (s[i] - s[i - 4] > gap) continue;
			positions.set([s[i - 3], s[i - 2], s[i - 1], s[i + 1], s[i + 2], s[i + 3]], k * 6);
			times[k * 2] = s[i - 4];
			times[k * 2 + 1] = s[i];
			owners[k * 2] = owners[k * 2 + 1] = index;
			k++;
		}
	});
	return { positions, times, owners };
}

/**
 * Heat map timelapse frames from per-day cells [{day, x, z, events}]: one frame per day from `from` to `to`, each
 * summing the `window` days up to and including it. Returns {cells: [[x, z]...] (every cell that ever has events),
 * frames: [{day, values: Float64Array (per cell), total}], max (largest value in any frame)}.
 */
export function heatFrames(rows, from, to, window = 1) {
	const index = new Map(), cells = [];
	const perDay = new Map();
	for (const r of rows) {
		const key = r.x + ',' + r.z;
		if (!index.has(key)) { index.set(key, cells.length); cells.push([r.x, r.z]); }
		if (!perDay.has(r.day)) perDay.set(r.day, []);
		perDay.get(r.day).push([index.get(key), r.events]);
	}
	const frames = [];
	let max = 0;
	const running = new Float64Array(cells.length);
	let total = 0;
	const start = from - window + 1;
	for (let day = start; day <= to; day++) {
		for (const [i, events] of perDay.get(day) || []) { running[i] += events; total += events; }
		const leaving = day - window;
		if (leaving >= start) for (const [i, events] of perDay.get(leaving) || []) { running[i] -= events; total -= events; }
		if (day < from) continue;
		const values = Float64Array.from(running);
		for (const v of values) if (v > max) max = v;
		frames.push({ day, values, total });
	}
	return { cells, frames, max };
}

/** 0..1 on a log scale, so a few busy cells don't wash out the rest. */
export function heatLevel(value, max) {
	if (value <= 0 || max <= 0) return 0;
	return Math.log1p(value) / Math.log1p(max);
}

/** A sequential colour (one hue, light to dark) as [r, g, b] 0..1 for a level 0..1. */
export function heatColor(level, light = [0xfd, 0xd9, 0xb5], dark = [0xa3, 0x3b, 0x06]) {
	const f = Math.max(0, Math.min(1, level));
	return light.map((c, i) => (c + (dark[i] - c) * f) / 255);
}

/** "1h 5m", "42s": a replay's length or the trail's. */
export function formatSpan(seconds) {
	seconds = Math.max(0, Math.round(seconds));
	const d = Math.floor(seconds / 86400), h = Math.floor(seconds % 86400 / 3600), m = Math.floor(seconds % 3600 / 60), s = seconds % 60;
	if (d) return d + 'd ' + h + 'h';
	if (h) return h + 'h ' + m + 'm';
	if (m) return m + 'm' + (s ? ' ' + s + 's' : '');
	return s + 's';
}

/**
 * The box around most of the points (flat [x, y, z, ...]): `trim` of them are dropped at each end of each axis, so a
 * few stray objects far away don't make everything else tiny. Null without points.
 */
export function coreBounds(pos, trim = 0.02) {
	const n = Math.floor(pos.length / 3);
	if (!n) return null;
	const cut = n >= 20 ? Math.floor(n * trim) : 0;
	const min = [], max = [];
	for (let axis = 0; axis < 3; axis++) {
		const values = [];
		for (let i = 0; i < n; i++) values.push(pos[i * 3 + axis]);
		values.sort((a, b) => a - b);
		min.push(values[cut]);
		max.push(values[n - 1 - cut]);
	}
	return { min, max };
}

/**
 * Some zones' terrain is only a flat plane the game never shows (Venture Explorer's floats thousands of units under
 * the ship). Terrain counts as that placeholder when it is flat (less than `flat` units from lowest to highest) and
 * the zone's objects are nowhere near its height (their middle height more than `away` units off).
 */
export function isPlaceholderTerrain(minY, maxY, objectMiddleY, flat = 1, away = 50) {
	if (!(maxY - minY < flat) || objectMiddleY === null || objectMiddleY === undefined) return false;
	return Math.abs(objectMiddleY - (minY + maxY) / 2) > away;
}

// ---- following a player across worlds ----

/**
 * Live view: where the followed player went, from one player_positions report (it lists every world's players).
 * Returns {zone, instance, clone} when they are reported only in a world the view isn't showing (another zone, or
 * another instance when one instance is picked; `instance` 0 is all of them), else null: while the old world still
 * reports them too, or before the new one does, the view stays. Character select (zone 0) has nothing to draw.
 */
export function followedMove(followId, players, zone, instance) {
	if (!followId) return null;
	let here = false, elsewhere = null;
	for (const p of players || []) {
		if (p.id !== followId) continue;
		if (p.zone === zone && (!instance || p.instance === instance)) here = true;
		else if (p.zone) elsewhere = p;
	}
	return !here && elsewhere ? { zone: elsewhere.zone, instance: elsewhere.instance, clone: elsewhere.clone } : null;
}

/**
 * Capture playback: the world a character was on at time t, from a capture's world moves ([{character, t, zone,
 * instance, clone, ...}] in time order, the positions route's `worlds`). Character select (zone 0) in between keeps the
 * world before it; before their first world, null.
 */
export function worldAt(worlds, character, t) {
	let found = null;
	for (const w of worlds || []) {
		if (w.t > t) break;
		if (w.character === character && w.zone) found = w;
	}
	return found;
}

/** Capture playback: the world to switch the 3D view to so it shows the followed character at time t, or null. */
export function captureSwitch(worlds, character, t, zone) {
	if (!character) return null;
	const w = worldAt(worlds, character, t);
	return w && w.zone !== zone ? w : null;
}

/**
 * Timeline markers for a capture's world changes: each time a character (every one, or only `character`) reaches a
 * world other than the one they were last in. Character select in between isn't a change; their first world is one
 * when the capture saw them before it (logging in from character select). Returns [{t, at (0 to 1 along
 * `duration` seconds), character, name, zone, zoneName, instance}].
 */
export function worldMarkers(worlds, duration, character = null) {
	const last = new Map(), out = [];
	for (const w of worlds || []) {
		if (character && w.character !== character) continue;
		const seen = last.has(w.character), before = last.get(w.character);
		if (!w.zone) { if (!seen) last.set(w.character, null); continue; }
		last.set(w.character, w);
		if (!seen || (before && before.zone === w.zone && before.instance === w.instance)) continue;
		out.push({ t: w.t, at: duration > 0 ? Math.max(0, Math.min(1, w.t / duration)) : 0, character: w.character, name: w.name,
			zone: w.zone, zoneName: w.zoneName, instance: w.instance });
	}
	return out;
}

/**
 * The markers over a timeline's slider: one tick per world change, labelled with the zone's name (and the character's,
 * when `names`); clicking one seeks to it (data-t). `esc` escapes text for HTML.
 */
export function markersHtml(markers, esc, names = false) {
	return markers.map((m) => {
		const zone = m.zoneName || ('Zone ' + m.zone);
		const title = (m.name ? m.name + ' → ' : '') + zone + ' #' + m.instance;
		return '<button type="button" class="timeline-marker" style="left:' + (m.at * 100).toFixed(3) + '%" data-t="' + m.t + '" title="' + esc(title) + '">' +
			'<span>' + esc(names && m.name ? m.name + ': ' + zone : zone) + '</span></button>';
	}).join('');
}
