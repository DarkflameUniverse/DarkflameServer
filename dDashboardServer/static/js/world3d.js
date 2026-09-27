/**
 * Live 3D world view (/world3d, ES module, three.js).
 *
 * A zone's terrain (the game's chunks and textures, lddviewer.js, or one layer of its terrain file: colour map, blend
 * map or scene map), its models and sky (scenery.js, on by default), the terrain's flairs, its scene objects as small
 * markers coloured by kind (one InstancedMesh per kind; objects drawn with a model get none unless asked), its paths
 * as lines (loaded when first shown) and its spawn points, with the players on it:
 *  - Live: positions from the player_positions socket topic (worlds report about once a second), drawn a second in
 *    the past so each player glides between two reports (world3d-core.js Track).
 *  - Replay (players_history): recorded positions over a time range, with a scrubber, play/pause, speed and trails.
 *    Trails are one LineSegments whose shader hides what is in the future or older than the trail length, so
 *    scrubbing never rebuilds geometry.
 *  - Heat map (reports_view): the economy map events of one kind per day as coloured 4x4 squares on the ground,
 *    played back day by day.
 * Names come from the server (kinds, path types, map event kinds); nothing about the game is kept here.
 */
import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
import { RoomEnvironment } from 'three/addons/environments/RoomEnvironment.js';
import { buildTerrainChunks, TERRAIN_LOOKS } from '/js/lddviewer.js';
import { createScenery } from '/js/scenery.js';
import { nearPlaneFor } from '/js/scenery-core.js';
import { Track, replayPosition, trailSegments, heatFrames, heatLevel, heatColor, formatSpan, coreBounds, isPlaceholderTerrain } from '/js/world3d-core.js';

const LIVE_DELAY = 1.2;        // seconds live players are drawn behind the newest report
const LIVE_FORGET = 8;         // seconds without a report before a live player is dropped
const MAX_LABELS = 60;
const SPAWN_LABEL_RANGE = 150; // spawn point names show within this distance of the camera
const PICK_PIXELS = 16;
// Categorical slots in fixed order for object kinds (by the kind's index from the server); "Other" is grey
const KIND_COLORS = ['#e34948', '#eda100', '#2a78d6', '#1baf7a', '#e87ba4', '#9085e9', '#eb6834', '#008300'];
const OTHER_COLOR = '#898781';
const PLAYER_COLOR = 0xf5f5f5, PICKED_COLOR = 0xffd166, PATH_COLOR = 0x33d6ff, SPAWN_COLOR = 0xc3c2b7;

const $ = (id) => document.getElementById(id);
const canHistory = !!$('modeReplay'), canHeat = !!$('modeHeat');

// ---- three.js scene ----

const container = $('world');
const renderer = new THREE.WebGLRenderer({ antialias: true });
renderer.setPixelRatio(Math.min(window.devicePixelRatio, 1.5));
container.appendChild(renderer.domElement);
const scene = new THREE.Scene();
scene.background = new THREE.Color(0x1e2126);
// Soft all-round light for the models (as the property view has); the terrain and markers light themselves
scene.environment = new THREE.PMREMGenerator(renderer).fromScene(new RoomEnvironment(), 0.04).texture;
scene.add(new THREE.HemisphereLight(0xdde8ff, 0x3a4030, 1.6));
const sun = new THREE.DirectionalLight(0xffffff, 1.4);
sun.position.set(0.5, 1, 0.3);
scene.add(sun);
const camera = new THREE.PerspectiveCamera(50, 1, 0.5, 20000);
const controls = new OrbitControls(camera, renderer.domElement);
controls.enableDamping = true;
controls.maxPolarAngle = Math.PI * 0.495;

function resize() {
	const width = container.clientWidth, height = container.clientHeight || 520;
	camera.aspect = width / height;
	camera.updateProjectionMatrix();
	renderer.setSize(width, height);
}
new ResizeObserver(resize).observe(container);
// The wheel only zooms the view, never scrolls the page
container.addEventListener('wheel', (e) => e.preventDefault(), { passive: false });
resize();

// The zone's full scenery (every object's model and the sky), when switched on
const scenery = createScenery({
	scene, camera, renderer,
	urls: {
		mesh: (zone, asset, lod) => '/api/scenery/' + zone + '/mesh/' + asset + '?lod=' + lod,
		texture: (zone, asset, slot, lod) => '/api/scenery/' + zone + '/texture/' + asset + '/' + slot + '?lod=' + lod
	},
	focus: (out) => out.copy(controls.target),
	onProgress: (loaded, wanted) => setStatus(loaded < wanted ? 'Scenery ' + loaded + '/' + wanted + '…' : ''),
	onScenes: renderScenes
});

// The near plane follows how far out the camera is, so far views keep their depth precision (no fighting surfaces)
function fitNearPlane(cam, target) {
	const near = nearPlaneFor(cam.position.distanceTo(target));
	if (Math.abs(near - cam.near) / cam.near < 0.15) return;
	cam.near = near;
	cam.updateProjectionMatrix();
}

// ---- scenes: which of the zone's scenes the scenery shows (scenery.js setSceneMode) ----

function renderScenes(info) {
	const list = $('sceneList'), status = $('sceneStatus');
	if (!list) return;
	const scenes = info.scenes || [];
	if (!scenes.length) {
		list.innerHTML = '';
		status.textContent = 'The zone file lists no scenes.';
		return;
	}
	const current = scenes.find((s) => s.id === info.scene);
	status.textContent = info.scene === null ? 'No scene map in the terrain file: every object counts as loaded.' :
		'Under the ' + (state.follow ? 'followed player' : 'camera') + ': ' + (current ? current.name + ' (' + current.id + ')' : 'the global scene');
	list.innerHTML = scenes.map((s) => {
		const shown = !info.shown || info.shown.has(s.id);
		const connected = current && current.neighbours.includes(s.id);
		return '<div class="form-check mb-0"><input class="form-check-input" type="checkbox" data-scene="' + esc(s.id) + '" id="scene' + esc(s.id) + '"' + (shown ? ' checked' : '') + '>' +
			'<label class="form-check-label" for="scene' + esc(s.id) + '">' + esc(s.name || 'Scene ' + s.id) + ' <span class="text-body-secondary">' + esc(s.id) +
			(s.id === info.scene ? ' · here' : connected ? ' · connected' : s.id === 0 ? ' · always' : '') + '</span></label></div>';
	}).join('');
}

$('sceneMode').addEventListener('change', () => scenery.setSceneMode($('sceneMode').value));
$('sceneList').addEventListener('change', (e) => {
	if (!e.target.dataset.scene) return;
	// Picking a scene by hand starts from what's shown now
	const picked = [...$('sceneList').querySelectorAll('input[data-scene]')].filter((i) => i.checked).map((i) => Number(i.dataset.scene));
	scenery.setManualScenes(picked);
	if ($('sceneMode').value !== 'manual') {
		$('sceneMode').value = 'manual';
		$('sceneMode').dispatchEvent(new Event('change'));
	}
});
// The terrain's flairs (grass, flowers, small rocks): models too, drawn only near the camera as the game does
const flairs = createScenery({
	scene, camera, renderer,
	urls: {
		mesh: (zone, asset, lod) => '/api/scenery/' + zone + '/mesh/' + asset + '?lod=' + lod,
		texture: (zone, asset, slot, lod) => '/api/scenery/' + zone + '/texture/' + asset + '/' + slot + '?lod=' + lod
	},
	focus: (out) => out.copy(controls.target)
});
let sceneryShown = null, flairsShown = null; // "zone/detail" loaded
function showScenery() {
	const on = $('sceneryToggle').checked, flairsOn = on && $('flairToggle').checked;
	scenery.setEnabled(on);
	scenery.setSky($('skyToggle').checked);
	scenery.setFog($('fogToggle').checked);
	flairs.setFog($('fogToggle').checked);
	scenery.setShowHidden($('hiddenToggle').checked);
	flairs.setEnabled(flairsOn);
	applyMarkers();
	if (!state.zone) return;
	const detail = parseInt($('sceneryDetail').value, 10);
	const key = state.zone + '/' + detail;
	if (on && sceneryShown !== key) {
		sceneryShown = key;
		scenery.load('/api/world3d/' + state.zone + '/scenery', detail).then((ok) => {
			if (!ok) return setStatus('No models for this zone');
			scenery.setSceneMode($('sceneMode').value);
		});
	}
	if (flairsOn && flairsShown !== key) {
		flairsShown = key;
		flairs.load('/api/world3d/' + state.zone + '/flairs', detail).then((ok) => {
			const count = ok ? flairs.count() : 0;
			$('flairCount').textContent = count ? count.toLocaleString() : '';
		});
	}
}

const layers = { terrain: new THREE.Group(), objects: new THREE.Group(), paths: new THREE.Group(), spawns: new THREE.Group(), heat: new THREE.Group() };
Object.values(layers).forEach((g) => scene.add(g));

function disposeGroup(group) {
	for (const child of [...group.children]) {
		group.remove(child);
		child.traverse((o) => {
			if (o.geometry) o.geometry.dispose();
			if (!o.material) return;
			for (const u of Object.values(o.material.uniforms || {})) if (u.value && u.value.isTexture && !u.value.userData.shared) u.value.dispose();
			o.material.dispose();
		});
	}
}

// ---- state ----

const state = {
	meta: null, zone: 0, instance: 0, mode: 'live',
	scene: null, heightAt: () => null, groundY: 0, bounds: new THREE.Box3(),
	objectMeshes: [], pathLines: new Map(), spawnPoints: [],
	paths: null,      // the zone's paths once loaded (/paths), and the layers of its terrain file (/terrain_layers)
	terrainLayers: null,
	selected: null,   // {type: 'player', id} or {type: 'object', index} or {type: 'heat', cell}
	follow: null,     // player id the camera tracks
	live: new Map(),  // id -> {id, name, instance, clone, track}
	replay: null,     // {from, to, players, gap, hold, t, playing, trails}
	heat: null        // {data, frames, cells, index, playing, mesh}
};

function setStatus(text) { $('worldStatus').textContent = text || ''; }
const zoneName = (id) => (state.meta && state.meta.zones[String(id)]) || ('Zone ' + id);

// ---- players (one InstancedMesh, grown when needed) ----

const playerGeometry = new THREE.CapsuleGeometry(0.55, 1.3, 3, 8);
const playerMaterial = new THREE.MeshLambertMaterial({ color: 0xffffff });
let playerMesh = null;
let drawnPlayers = []; // [{id, name, x, y, z}] drawn this frame, by instance index
function ensurePlayerCapacity(n) {
	if (playerMesh && playerMesh.userData.capacity >= n) return;
	const capacity = Math.max(64, 2 ** Math.ceil(Math.log2(Math.max(n, 1))));
	if (playerMesh) { scene.remove(playerMesh); playerMesh.dispose(); }
	playerMesh = new THREE.InstancedMesh(playerGeometry, playerMaterial, capacity);
	playerMesh.userData.capacity = capacity;
	playerMesh.count = 0;
	playerMesh.frustumCulled = false;
	playerMesh.setColorAt(0, new THREE.Color(PLAYER_COLOR));
	scene.add(playerMesh);
}
ensurePlayerCapacity(64);

const matrix = new THREE.Matrix4(), color = new THREE.Color(), point = {};
function drawPlayers(list) {
	ensurePlayerCapacity(list.length);
	list.forEach((p, i) => {
		matrix.makeTranslation(p.x, p.y + 1.2, p.z);
		playerMesh.setMatrixAt(i, matrix);
		const picked = state.selected && state.selected.type === 'player' && state.selected.id === p.id;
		playerMesh.setColorAt(i, color.set(picked || state.follow === p.id ? PICKED_COLOR : PLAYER_COLOR));
	});
	playerMesh.count = list.length;
	playerMesh.instanceMatrix.needsUpdate = true;
	if (playerMesh.instanceColor) playerMesh.instanceColor.needsUpdate = true;
	drawnPlayers = list;
}

function currentPlayers(nowSeconds) {
	const list = [];
	if (state.mode === 'replay' && state.replay) {
		const r = state.replay;
		for (const p of r.players) {
			if (!replayPosition(p.samples, r.t, r.gap, r.hold, point)) continue;
			list.push({ id: p.id, name: p.name, x: point.x, y: point.y, z: point.z });
		}
	} else if (state.mode === 'live') {
		const t = nowSeconds - LIVE_DELAY;
		for (const p of state.live.values()) {
			if (!p.track.sample(t, point)) continue;
			list.push({ id: p.id, name: p.name, x: point.x, y: point.y, z: point.z });
		}
	}
	return list;
}

// ---- live positions ----

function onPositions(players) {
	const now = performance.now() / 1000;
	for (const p of players || []) {
		if (p.zone !== state.zone || (state.instance && p.instance !== state.instance)) continue;
		let entry = state.live.get(p.id);
		if (!entry) state.live.set(p.id, entry = { id: p.id, name: p.name, track: new Track() });
		entry.instance = p.instance;
		entry.clone = p.clone;
		entry.seen = now;
		entry.track.push(now, p.x, p.y, p.z);
	}
	for (const [id, entry] of state.live) if (now - entry.seen > LIVE_FORGET) state.live.delete(id);
	noteInstances(players);
	renderPlayerList();
}

// ---- zone scene: objects, paths, spawn points ----

const kindGeometry = new THREE.OctahedronGeometry(0.9);
const otherGeometry = new THREE.BoxGeometry(0.7, 0.7, 0.7);

function kindVisible(value) { return Prefs.get('world3d.kind.' + value, true); }
function pathVisible(value) { return Prefs.get('world3d.path.' + value, true); }

function buildObjects() {
	disposeGroup(layers.objects);
	state.objectMeshes = [];
	const s = state.scene;
	if (!s) return;
	const o = s.objects, other = s.kinds.length - 1;
	const clientOnly = $('clientOnlyToggle').checked;
	const perKind = s.kinds.map(() => []);
	for (let i = 0; i < o.lot.length; i++) {
		if (!clientOnly && (o.flags[i] & 2)) continue;
		perKind[o.kind[i]].push(i);
	}
	perKind.forEach((indices, k) => {
		// Objects the scenery draws with a model get their own mesh, so their markers can be hidden
		for (const modeled of [false, true]) {
			const mine = indices.filter((i) => !!(o.flags[i] & 4) === modeled);
			if (!mine.length) continue;
			const mesh = new THREE.InstancedMesh(k === other ? otherGeometry : kindGeometry,
				new THREE.MeshLambertMaterial({ color: k === other ? OTHER_COLOR : KIND_COLORS[k % KIND_COLORS.length] }), mine.length);
			mine.forEach((index, i) => {
				matrix.makeTranslation(o.pos[index * 3], o.pos[index * 3 + 1] + 0.9, o.pos[index * 3 + 2]);
				mesh.setMatrixAt(i, matrix);
			});
			mesh.computeBoundingSphere();
			mesh.userData = { kind: k, indices: mine, modeled };
			mesh.visible = kindVisible(s.kinds[k].value);
			layers.objects.add(mesh);
			state.objectMeshes.push(mesh);
		}
	});
	applyMarkers();
	renderLayerList(perKind);
}

// With models shown, an object drawn with its model has no marker unless asked for; it stays clickable (the ray still
// hits the unseen marker)
function applyMarkers() {
	const hide = $('sceneryToggle').checked && !$('modelMarkersToggle').checked;
	for (const mesh of state.objectMeshes) mesh.material.visible = !(mesh.userData.modeled && hide);
}

// Paths are most of a zone file, so they are only fetched once the paths layer is switched on
async function showPaths() {
	const on = $('pathsToggle').checked;
	layers.paths.visible = on;
	if (!on || !state.zone) { renderPathList(); return; }
	if (!state.paths) {
		const zone = state.zone;
		setStatus('Loading paths…');
		const data = await api.get('/api/world3d/' + zone + '/paths').catch(() => null);
		setStatus('');
		if (zone !== state.zone) return;
		state.paths = data && data.paths ? data : { paths: [], pathTypes: [] };
		buildPaths();
	}
	renderPathList();
}

function buildPaths() {
	disposeGroup(layers.paths);
	state.pathLines = new Map();
	const s = state.paths;
	if (!s) return;
	const byType = new Map();
	for (const path of s.paths) {
		const p = path.points, n = p.length / 3;
		if (n < 2) continue;
		if (!byType.has(path.type)) byType.set(path.type, []);
		const out = byType.get(path.type);
		const segments = path.loop && n > 2 ? n : n - 1;
		for (let i = 0; i < segments; i++) {
			const a = i * 3, b = ((i + 1) % n) * 3;
			out.push(p[a], p[a + 1] + 0.3, p[a + 2], p[b], p[b + 1] + 0.3, p[b + 2]);
		}
	}
	for (const [type, positions] of byType) {
		const geometry = new THREE.BufferGeometry();
		geometry.setAttribute('position', new THREE.Float32BufferAttribute(positions, 3));
		const line = new THREE.LineSegments(geometry, new THREE.LineBasicMaterial({ color: PATH_COLOR, transparent: true, opacity: 0.8 }));
		line.visible = pathVisible(type);
		line.userData.count = s.paths.filter((p) => p.type === type).length;
		layers.paths.add(line);
		state.pathLines.set(type, line);
	}
}

function buildSpawns() {
	disposeGroup(layers.spawns);
	const s = state.scene;
	state.spawnPoints = s ? s.spawnPoints.slice() : [];
	if (s && s.spawn) state.spawnPoints.push({ name: 'Zone spawn', x: s.spawn[0], y: s.spawn[1], z: s.spawn[2] });
	$('spawnCount').textContent = state.spawnPoints.length ? '(' + state.spawnPoints.length + ')' : '';
	if (!state.spawnPoints.length) return;
	const mesh = new THREE.InstancedMesh(new THREE.ConeGeometry(0.6, 1.8, 8), new THREE.MeshLambertMaterial({ color: SPAWN_COLOR }), state.spawnPoints.length);
	state.spawnPoints.forEach((p, i) => { matrix.makeTranslation(p.x, p.y + 0.9, p.z); mesh.setMatrixAt(i, matrix); });
	mesh.computeBoundingSphere();
	layers.spawns.add(mesh);
	layers.spawns.visible = $('spawnToggle').checked;
}

// The middle height of the zone's objects: where heat cells and players go without terrain under them
function objectMiddleY() {
	if (!state.scene) return null;
	const ys = placedPositions(state.scene.objects.pos).filter((_, i) => i % 3 === 1).sort((a, b) => a - b);
	return ys.length ? ys[ys.length >> 1] : null;
}

// Objects left at exactly the origin are zone-wide things (Venture Explorer's asteroid belt, its NPCs' parts), not places
function placedPositions(pos) {
	const out = [];
	for (let i = 0; i + 2 < pos.length; i += 3) {
		if (pos[i] !== 0 || pos[i + 1] !== 0 || pos[i + 2] !== 0) out.push(pos[i], pos[i + 1], pos[i + 2]);
	}
	return out;
}

// What the view starts on: the zone's objects (leaving out a few strays far away) and its spawn point; the terrain
// only when there are no objects, since some zones' terrain reaches far past where anything is
function computeBounds() {
	const box = new THREE.Box3();
	const core = state.scene ? coreBounds(placedPositions(state.scene.objects.pos), 0.05) : null;
	if (core) {
		box.min.fromArray(core.min);
		box.max.fromArray(core.max);
		if (state.scene.spawn) box.expandByPoint(new THREE.Vector3().fromArray(state.scene.spawn));
	} else if (layers.terrain.children.length) {
		box.setFromObject(layers.terrain);
	}
	state.bounds = box;
}

function frameAll() {
	const box = state.bounds;
	if (box.isEmpty()) { camera.position.set(80, 80, 80); controls.target.set(0, 0, 0); return; }
	const center = box.getCenter(new THREE.Vector3()), size = box.getSize(new THREE.Vector3());
	const extent = Math.max(size.x, size.y, size.z, 40);
	controls.target.copy(center);
	camera.position.copy(center).add(new THREE.Vector3(extent * 0.35, extent * 0.55, extent * 0.35));
	camera.far = extent * 8;
	camera.updateProjectionMatrix();
}

let terrainData = null;
function showTerrain() {
	disposeGroup(layers.terrain);
	if (state.terrain) state.terrain.dispose();
	state.terrain = null;
	state.heightAt = () => null;
	state.placeholderTerrain = false;
	if (!terrainData || !$('terrainToggle').checked) return;
	const loader = new THREE.TextureLoader(), textures = new Map();
	const loadTexture = (id) => {
		if (!textures.has(id)) {
			const texture = loader.load('/api/terrain_textures/' + id, undefined, undefined, () => {});
			texture.wrapS = texture.wrapT = THREE.RepeatWrapping;
			texture.anisotropy = 4;
			textures.set(id, texture);
		}
		return textures.get(id);
	};
	const built = buildTerrainChunks(terrainData, loadTexture, sun.position.clone().normalize(), scenery.gameLights());
	// A flat plane far from every object is a placeholder the game never shows: leave it out, and don't put heat
	// cells or players on it
	const terrainBox = new THREE.Box3().setFromObject(built.group);
	if (isPlaceholderTerrain(terrainBox.min.y, terrainBox.max.y, objectMiddleY())) {
		state.placeholderTerrain = true;
		disposeGroup(built.group);
		textures.forEach((texture) => texture.dispose());
		return;
	}
	layers.terrain.add(built.group);
	state.heightAt = built.heightAt;
	state.terrain = built;
	showTerrainLook();
}

// Which layer of the terrain file the ground shows; the scene map is fetched the first time it's asked for
async function showTerrainLook() {
	const look = Number($('terrainLook').value);
	const built = state.terrain;
	if (look === TERRAIN_LOOKS.scenes && !state.terrainLayers && state.zone) {
		const zone = state.zone;
		setStatus('Loading the scene map…');
		const data = await api.get('/api/world3d/' + zone + '/terrain_layers').catch(() => null);
		setStatus('');
		if (zone !== state.zone) return;
		state.terrainLayers = data && data.chunks ? data : { scenes: [], chunks: [] };
	}
	if (built && built === state.terrain) built.setLook(look, state.terrainLayers);
	renderSceneLegend();
}

function renderSceneLegend() {
	const layers = state.terrainLayers, el = $('sceneLegend');
	if (Number($('terrainLook').value) !== TERRAIN_LOOKS.scenes || !layers) { el.innerHTML = ''; return; }
	if (!layers.scenes.length) { el.innerHTML = '<span class="text-body-secondary">The terrain file has no scene map.</span>'; return; }
	const scenes = layers.scenes.slice().sort((a, b) => b.share - a.share);
	el.innerHTML = '<div class="text-body-secondary mb-1">Scenes on the terrain (share of its area)</div><div class="world-legend">' + scenes.map((scene) =>
		'<div><span class="world-swatch" style="background:rgb(' + scene.color.join(',') + ')"></span>' + esc(scene.name) +
		' <span class="text-body-secondary">' + esc(scene.id) + ' · ' + (scene.share * 100).toFixed(scene.share < 0.01 ? 2 : 1) + '%</span></div>').join('') + '</div>';
}

let loadToken = 0;
async function loadZone(zone) {
	const token = ++loadToken;
	state.zone = zone;
	state.live.clear();
	state.selected = null;
	state.follow = null;
	stopReplay();
	clearHeat();
	// The old zone goes at once: its models, flairs, terrain and markers, and whatever of them is still loading
	scenery.clear();
	flairs.clear();
	sceneryShown = flairsShown = null;
	terrainData = null;
	state.scene = null;
	showTerrain();
	buildObjects();
	disposeGroup(layers.paths);
	state.pathLines = new Map();
	buildSpawns();
	if (canHeat) loadHeatProperties(zone);
	setStatus('Loading ' + zoneName(zone) + '…');
	const [sceneData, terrain] = await Promise.all([
		api.get('/api/world3d/' + zone + '/scene').catch(() => null),
		api.get('/api/world3d/' + zone + '/terrain_chunks').catch(() => null)
	]);
	if (token !== loadToken) return;
	state.scene = sceneData && sceneData.objects ? sceneData : null;
	state.paths = null;
	state.terrainLayers = null;
	disposeGroup(layers.paths);
	state.pathLines = new Map();
	$('flairCount').textContent = '';
	terrainData = terrain && terrain.chunks && terrain.chunks.length ? terrain : null;
	state.groundY = objectMiddleY() ?? 0;
	showTerrain();
	buildObjects();
	showPaths();
	buildSpawns();
	computeBounds();
	frameAll();
	const objects = state.scene ? state.scene.objects.lot.length : 0;
	const modeled = state.scene ? state.scene.objects.flags.filter((f) => f & 4).length : 0;
	$('worldMeta').textContent = state.scene ? objects.toLocaleString() + ' objects (' + modeled.toLocaleString() + ' with a model)' + (!terrainData ? ', no terrain' : state.placeholderTerrain ? ', terrain is a flat placeholder far from the objects (not drawn)' : '') : '';
	setStatus(state.scene ? '' : 'No client files for this zone (is client_location set?)');
	renderInstances();
	renderSelection();
	showScenery();
	api.get('/api/live/players').then((d) => onPositions(d.players)).catch(() => {});
	if (state.mode === 'replay') loadReplayInstances();
}

// ---- zones and instances ----

const seenInstances = new Map(); // zone -> Map(instance -> {clone, players})
function noteInstances(players) {
	let changed = false;
	for (const p of players || []) {
		if (!seenInstances.has(p.zone)) seenInstances.set(p.zone, new Map());
		const zone = seenInstances.get(p.zone);
		if (!zone.has(p.instance)) { zone.set(p.instance, { clone: p.clone, players: 0 }); changed = true; }
	}
	if (changed) renderInstances();
}

function renderInstances() {
	const select = $('instanceSelect');
	const instances = new Map();
	for (const w of (state.meta && state.meta.worlds) || []) if (w.zone === state.zone) instances.set(w.instance, { clone: w.clone, players: w.players });
	for (const [instance, info] of seenInstances.get(state.zone) || []) if (!instances.has(instance)) instances.set(instance, info);
	if (state.mode === 'replay' && state.replayInstances) for (const i of state.replayInstances) if (!instances.has(i.instance)) instances.set(i.instance, { clone: i.clone, recorded: true });
	const options = ['<option value="0">All instances</option>'];
	[...instances.keys()].sort((a, b) => a - b).forEach((instance) => {
		const info = instances.get(instance);
		options.push('<option value="' + instance + '">Instance ' + instance + (info.clone ? ' (clone ' + info.clone + ')' : '') +
			(info.players !== undefined ? ' · ' + info.players + ' online' : info.recorded ? ' · recorded' : '') + '</option>');
	});
	select.innerHTML = options.join('');
	select.value = instances.has(state.instance) ? String(state.instance) : '0';
	state.instance = Number(select.value);
}

async function loadMeta() {
	state.meta = await api.get('/api/world3d/meta');
	const zones = state.meta.zones || {};
	const running = new Map();
	for (const w of state.meta.worlds) running.set(w.zone, (running.get(w.zone) || 0) + w.players);
	const option = (id) => '<option value="' + id + '">' + esc(zones[id] || 'Zone ' + id) + ' (' + id + ')' + (running.has(Number(id)) ? ' · ' + running.get(Number(id)) + ' online' : '') + '</option>';
	// Character select (zone 0) has no world to draw
	const ids = Object.keys(zones).filter((id) => Number(id) !== 0).sort((a, b) => Number(a) - Number(b));
	const live = ids.filter((id) => running.has(Number(id)));
	const select = $('zoneSelect'), previous = select.value || Prefs.get('world3d.zone', '');
	select.innerHTML = (live.length ? '<optgroup label="Running">' + live.map(option).join('') + '</optgroup>' : '') +
		'<optgroup label="Every zone">' + ids.map(option).join('') + '</optgroup>';
	if (previous && select.querySelector('option[value="' + CSS.escape(String(previous)) + '"]')) select.value = previous;
	else if (live.length) select.value = live.reduce((best, id) => running.get(Number(id)) > running.get(Number(best)) ? id : best); // the busiest
	if (canHeat) {
		const kind = $('heatKind'), saved = Prefs.get('world3d.heatKind', kind.value);
		kind.innerHTML = state.meta.mapKinds.map((k) => '<option value="' + k.value + '">' + esc(k.name) + '</option>').join('');
		if (saved && kind.querySelector('option[value="' + CSS.escape(String(saved)) + '"]')) kind.value = saved;
	}
}

// ---- panel: players, selection, layers ----

function listedPlayers() {
	if (state.mode === 'replay' && state.replay) return state.replay.players.map((p) => ({ id: p.id, name: p.name, instance: p.instances.join(', ') }));
	return [...state.live.values()].map((p) => ({ id: p.id, name: p.name, instance: p.instance }));
}

const renderPlayerList = window.Live && Live.throttle ? Live.throttle(renderPlayerListNow, 1000) : renderPlayerListNow;
function renderPlayerListNow() {
	const query = $('playerSearch').value.trim().toLowerCase();
	const players = listedPlayers().sort((a, b) => a.name.localeCompare(b.name));
	$('playerCount').textContent = players.length;
	const shown = players.filter((p) => !query || p.name.toLowerCase().includes(query));
	$('playerList').innerHTML = shown.map((p) =>
		'<a class="list-group-item list-group-item-action d-flex justify-content-between align-items-center px-2' +
		(state.selected && state.selected.type === 'player' && state.selected.id === p.id ? ' active' : '') + '" data-id="' + esc(p.id) + '">' +
		'<span>' + esc(p.name) + (state.follow === p.id ? ' <span class="badge text-bg-warning">following</span>' : '') + '</span>' +
		'<span class="text-body-secondary">' + esc(p.instance) + '</span></a>').join('') ||
		'<div class="text-body-secondary p-2">' + (state.mode === 'heat' ? 'Players are hidden while the heat map is shown.' :
			state.mode === 'replay' ? (state.replay ? 'Nobody was recorded here then.' : 'Pick a range and press Load.') : 'Nobody is online in this zone.') + '</div>';
}

function selectPlayer(id, follow) {
	state.selected = { type: 'player', id };
	if (follow !== undefined) state.follow = follow ? id : null;
	renderSelection();
	renderPlayerListNow();
}

function renderSelection() {
	const el = $('selectionInfo'), sel = state.selected;
	if (!sel) { el.innerHTML = '<span class="text-body-secondary">Click a player or an object.</span>'; return; }
	if (sel.type === 'player') {
		const live = state.live.get(sel.id);
		const replayed = state.replay && state.replay.players.find((p) => p.id === sel.id);
		const name = (live && live.name) || (replayed && replayed.name) || sel.id;
		const drawn = drawnPlayers.find((p) => p.id === sel.id);
		el.innerHTML = '<h6 class="mb-1">' + fmt.character(sel.id, name) + '</h6>' +
			'<div>' + fmt.zone(state.zone, zoneName(state.zone)) + '</div>' +
			(live ? '<div>Instance ' + esc(live.instance) + (live.clone ? ', clone ' + esc(live.clone) : '') + '</div>' : '') +
			(replayed ? '<div>Recorded in instance ' + esc(replayed.instances.join(', ')) + ', ' + (replayed.samples.length / 4) + ' samples</div>' : '') +
			'<div class="text-body-secondary" id="selectedPosition">' + (drawn ? positionText(drawn) : 'Not here right now') + '</div>' +
			'<div class="d-flex flex-wrap gap-1 mt-2">' +
			'<button class="btn btn-sm btn-outline-primary" data-act="follow">' + (state.follow === sel.id ? 'Stop following' : 'Follow') + '</button>' +
			'<button class="btn btn-sm btn-outline-secondary" data-act="focus">Look at</button>' +
			(canHistory && state.mode === 'live' ? '<button class="btn btn-sm btn-outline-secondary" data-act="replay">Replay last hour</button>' : '') + '</div>';
	} else if (sel.type === 'object') {
		const s = state.scene, o = s.objects, i = sel.index;
		const lot = o.lot[i], kind = s.kinds[o.kind[i]];
		const cdclient = DASH.can('dev_cdclient') ? ' <a href="/cdclient#/object/' + lot + '">CDClient</a>' : '';
		el.innerHTML = '<h6 class="mb-1">' + esc(s.lotNames[lot] || 'LOT ' + lot) + '</h6>' +
			'<div>LOT ' + esc(lot) + cdclient + '</div><div>' + esc(kind ? kind.name : '') +
			((o.flags[i] & 1) ? ', spawned by a spawner' : '') + ((o.flags[i] & 2) ? ', only the client loads it' : '') + '</div>' +
			(o.names[i] ? '<div>Name: ' + esc(o.names[i]) + '</div>' : '') +
			(s.scenes && s.scenes[o.scene[i]] !== undefined ? '<div>Scene: ' + esc(s.scenes[o.scene[i]]) + ' (' + esc(o.scene[i]) + ')</div>' : '') +
			((o.flags[i] & 4) ? '<div class="text-body-secondary">Drawn with its model</div>' : '') +
			'<div class="text-body-secondary">' + positionText({ x: o.pos[i * 3], y: o.pos[i * 3 + 1], z: o.pos[i * 3 + 2] }) + '</div>';
	} else if (sel.type === 'heat') {
		const h = state.heat, frame = h && h.frames[h.index];
		const [cx, cz] = h.cells[sel.cell], size = h.cellSize;
		el.innerHTML = '<h6 class="mb-1">' + esc(h.kindName) + '</h6><div>' + (frame ? Math.round(frame.values[sel.cell]).toLocaleString() : 0) +
			' on ' + esc(dayText(frame ? frame.day : 0)) + (h.window > 1 ? ' (' + (h.window > 1000 ? 'so far' : h.window + ' days') + ')' : '') + '</div>' +
			'<div class="text-body-secondary">Square x ' + cx * size + ' to ' + (cx + 1) * size + ', z ' + cz * size + ' to ' + (cz + 1) * size + '</div>';
	}
}

function positionText(p) { return 'x ' + p.x.toFixed(1) + ', y ' + p.y.toFixed(1) + ', z ' + p.z.toFixed(1); }

$('selectionInfo').addEventListener('click', (e) => {
	const act = e.target.closest('[data-act]');
	if (!act || !state.selected || state.selected.type !== 'player') return;
	const id = state.selected.id;
	if (act.dataset.act === 'follow') selectPlayer(id, state.follow !== id);
	else if (act.dataset.act === 'focus') focusPlayer(id);
	else if (act.dataset.act === 'replay') {
		const live = state.live.get(id);
		$('modeReplay').checked = true;
		$('replayRange').value = '3600';
		if (live) state.instance = live.instance;
		setMode('replay').then(() => loadReplay().then(() => selectPlayer(id, true)));
	}
});

$('playerList').addEventListener('click', (e) => {
	const item = e.target.closest('[data-id]');
	if (!item) return;
	selectPlayer(item.dataset.id);
	focusPlayer(item.dataset.id);
	showSelectedTab();
});
$('playerList').addEventListener('dblclick', (e) => {
	const item = e.target.closest('[data-id]');
	if (item) selectPlayer(item.dataset.id, true);
});
$('playerSearch').addEventListener('input', renderPlayerListNow);

function showSelectedTab() {
	if (window.bootstrap) bootstrap.Tab.getOrCreateInstance($('selectedTabBtn')).show();
}

function focusPlayer(id) {
	const p = drawnPlayers.find((d) => d.id === id);
	if (!p) return;
	const offset = camera.position.clone().sub(controls.target);
	if (offset.length() > 120) offset.setLength(60);
	controls.target.set(p.x, p.y, p.z);
	camera.position.copy(controls.target).add(offset);
}

function renderLayerList(perKind) {
	const s = state.scene;
	$('kindList').innerHTML = s ? s.kinds.map((k, i) => {
		const swatch = i === s.kinds.length - 1 ? OTHER_COLOR : KIND_COLORS[i % KIND_COLORS.length];
		return '<div class="form-check"><input class="form-check-input" type="checkbox" id="kind' + i + '" data-kind="' + i + '"' + (kindVisible(k.value) ? ' checked' : '') + '>' +
			'<label class="form-check-label" for="kind' + i + '"><span class="world-swatch" style="background:' + swatch + '"></span>' + esc(k.name) +
			' <span class="text-body-secondary">' + (perKind[i] ? perKind[i].length : 0).toLocaleString() + '</span></label></div>';
	}).join('') : '';
}

function renderPathList() {
	const s = state.paths;
	$('pathCount').textContent = s ? '(' + s.paths.length.toLocaleString() + ')' : '';
	if (!$('pathsToggle').checked || !s) { $('pathList').innerHTML = ''; return; }
	const types = s.pathTypes.filter((t) => s.paths.some((p) => p.type === t.value));
	$('pathList').innerHTML = types.map((t) =>
		'<div class="form-check"><input class="form-check-input" type="checkbox" id="path' + t.value + '" data-path="' + t.value + '"' + (pathVisible(t.value) ? ' checked' : '') + '>' +
		'<label class="form-check-label" for="path' + t.value + '">' + esc(t.name) + ' <span class="text-body-secondary">' +
		s.paths.filter((p) => p.type === t.value).length + '</span></label></div>').join('') || '<span class="text-body-secondary">None</span>';
}

$('kindList').addEventListener('change', (e) => {
	const i = e.target.dataset.kind;
	if (i === undefined) return;
	const value = state.scene.kinds[i].value;
	Prefs.set('world3d.kind.' + value, e.target.checked);
	for (const mesh of state.objectMeshes) if (mesh.userData.kind === Number(i)) mesh.visible = e.target.checked;
});
$('pathList').addEventListener('change', (e) => {
	const type = e.target.dataset.path;
	if (type === undefined) return;
	Prefs.set('world3d.path.' + type, e.target.checked);
	const line = state.pathLines.get(Number(type));
	if (line) line.visible = e.target.checked;
});
$('clientOnlyToggle').addEventListener('change', buildObjects);
$('spawnToggle').addEventListener('change', () => { layers.spawns.visible = $('spawnToggle').checked; });
$('sceneryToggle').addEventListener('change', showScenery);
$('sceneryDetail').addEventListener('change', showScenery);
$('flairToggle').addEventListener('change', showScenery);
$('skyToggle').addEventListener('change', showScenery);
$('fogToggle').addEventListener('change', showScenery);
$('hiddenToggle').addEventListener('change', showScenery);
$('modelMarkersToggle').addEventListener('change', applyMarkers);
$('pathsToggle').addEventListener('change', showPaths);
$('terrainLook').addEventListener('change', showTerrainLook);
$('terrainToggle').addEventListener('change', () => { showTerrain(); if (state.heat) placeHeat(); });

// ---- picking ----

const raycaster = new THREE.Raycaster();
function screenOf(x, y, z, out) {
	out.set(x, y, z).project(camera);
	return out.z < 1 && out.z > -1;
}
const projected = new THREE.Vector3();
function pick(event) {
	const rect = renderer.domElement.getBoundingClientRect();
	const px = event.clientX - rect.left, py = event.clientY - rect.top;
	// Players by distance on screen, so small far-away ones are easy to hit
	let best = null, bestDistance = PICK_PIXELS;
	for (const p of drawnPlayers) {
		if (!screenOf(p.x, p.y + 1.2, p.z, projected)) continue;
		const d = Math.hypot((projected.x + 1) / 2 * rect.width - px, (1 - projected.y) / 2 * rect.height - py);
		if (d < bestDistance) { bestDistance = d; best = p; }
	}
	if (best) return { type: 'player', id: best.id };
	raycaster.setFromCamera(new THREE.Vector2(px / rect.width * 2 - 1, -(py / rect.height) * 2 + 1), camera);
	if (state.heat && state.heat.mesh) {
		const hit = raycaster.intersectObject(state.heat.mesh, false)[0];
		if (hit && state.heat.visibleCells[hit.instanceId]) return { type: 'heat', cell: hit.instanceId };
	}
	const hit = raycaster.intersectObjects(state.objectMeshes.filter((m) => m.visible), false)[0];
	if (hit) return { type: 'object', index: hit.object.userData.indices[hit.instanceId] };
	return null;
}

let downAt = null;
renderer.domElement.addEventListener('pointerdown', (e) => { downAt = [e.clientX, e.clientY]; });
renderer.domElement.addEventListener('click', (e) => {
	if (!downAt || Math.hypot(e.clientX - downAt[0], e.clientY - downAt[1]) > 4) return;
	state.selected = pick(e);
	renderSelection();
	renderPlayerListNow();
	if (state.selected) showSelectedTab();
});
renderer.domElement.addEventListener('dblclick', (e) => {
	const picked = pick(e);
	if (picked && picked.type === 'player') { selectPlayer(picked.id, true); focusPlayer(picked.id); }
});

// ---- labels (a pool of divs over the canvas) ----

const labelLayer = $('labels');
const labelPool = [];
function label(i) {
	if (!labelPool[i]) {
		const el = document.createElement('div');
		el.className = 'world-label';
		labelLayer.appendChild(el);
		labelPool[i] = el;
	}
	return labelPool[i];
}

function updateLabels() {
	let used = 0;
	if ($('labelsToggle').checked) {
		const width = labelLayer.clientWidth, height = labelLayer.clientHeight;
		const candidates = [];
		for (const p of drawnPlayers) candidates.push({ text: p.name, x: p.x, y: p.y + 2.6, z: p.z, cls: 'world-label' + (state.follow === p.id ? ' picked' : ''), rank: 0 });
		if (layers.spawns.visible) {
			for (const s of state.spawnPoints) {
				if (camera.position.distanceTo(projected.set(s.x, s.y, s.z)) > SPAWN_LABEL_RANGE) continue;
				candidates.push({ text: s.name, x: s.x, y: s.y + 2.2, z: s.z, cls: 'world-label spawn', rank: 1 });
			}
		}
		for (const c of candidates) c.distance = camera.position.distanceTo(projected.set(c.x, c.y, c.z));
		candidates.sort((a, b) => a.rank - b.rank || a.distance - b.distance);
		for (const c of candidates) {
			if (used >= MAX_LABELS) break;
			if (!screenOf(c.x, c.y, c.z, projected)) continue;
			const sx = (projected.x + 1) / 2 * width, sy = (1 - projected.y) / 2 * height;
			if (sx < -50 || sy < -20 || sx > width + 50 || sy > height + 20) continue;
			const el = label(used++);
			if (el.textContent !== c.text) el.textContent = c.text;
			if (el.className !== c.cls) el.className = c.cls;
			el.style.transform = 'translate(' + sx.toFixed(1) + 'px,' + sy.toFixed(1) + 'px) translate(-50%,-100%)';
			el.style.display = '';
		}
	}
	for (let i = used; i < labelPool.length; i++) labelPool[i].style.display = 'none';
}

// ---- replay ----

const TRAIL_VERTEX = `
attribute float aTime;
attribute float aOwner;
uniform float uNow;
uniform float uSelected;
varying float vAge;
varying float vPicked;
void main() {
	vAge = uNow - aTime;
	vPicked = abs(aOwner - uSelected) < 0.5 ? 1.0 : 0.0;
	gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);
}`;
const TRAIL_FRAGMENT = `
uniform float uTrail;
varying float vAge;
varying float vPicked;
void main() {
	if (vAge < 0.0 || vAge > uTrail) discard;
	float fade = 1.0 - vAge / uTrail;
	vec3 color = mix(vec3(0.96), vec3(1.0, 0.82, 0.4), vPicked);
	gl_FragColor = vec4(color, (0.25 + 0.65 * fade) * (0.55 + 0.45 * vPicked));
}`;

function stopReplay() {
	if (!state.replay) return;
	if (state.replay.trails) {
		scene.remove(state.replay.trails);
		state.replay.trails.geometry.dispose();
		state.replay.trails.material.dispose();
	}
	state.replay = null;
}

async function loadReplayInstances() {
	const data = await api.get('/api/world3d/history/instances?zone=' + state.zone).catch(() => null);
	state.replayInstances = data && data.instances ? data.instances : [];
	renderInstances();
}

function replayRange() {
	const range = $('replayRange').value;
	if (range !== 'custom') {
		const to = Math.floor(Date.now() / 1000);
		return { from: to - Number(range), to };
	}
	const from = Date.parse($('replayFrom').value), to = Date.parse($('replayTo').value);
	return isNaN(from) || isNaN(to) ? null : { from: Math.floor(from / 1000), to: Math.floor(to / 1000) };
}

async function loadReplay() {
	const range = replayRange();
	if (!range || range.to <= range.from) { toast('Pick when the replay starts and ends', 'warning'); return; }
	setStatus('Loading replay…');
	const data = await api.get('/api/world3d/history?zone=' + state.zone + '&instance=' + state.instance + '&from=' + range.from + '&to=' + range.to).catch(() => null);
	setStatus('');
	if (!data || data.success === false) { toast((data && data.error) || 'Could not load the replay', 'danger'); return; }
	startReplay(data);
}

/**
 * A packet capture's movement (the Packet Captures page's "World 3D" button: /world3d?capture=<id>&zone=<zone>): the
 * captured characters' position updates, replayed like recorded positions. The capture page's timeline drives the
 * time here while it plays (BroadcastChannel "dlu-capture-replay").
 */
async function loadCaptureReplay(capture) {
	setStatus('Loading the capture…');
	const data = await api.get('/api/inspector/sessions/' + encodeURIComponent(capture) + '/positions?zone=' + state.zone).catch(() => null);
	setStatus('');
	if (!data || data.success === false) { toast((data && data.error) || 'Could not load the capture', 'danger'); return; }
	startReplay(data);
	state.replay.capture = String(capture);
	if (!data.players.length) toast('No movement was captured in this zone' + (data.zones && data.zones.length ? ' (captured in: ' + data.zones.map(zoneName).join(', ') + ')' : ''), 'info');
}

function startReplay(data) {
	stopReplay();
	const gap = Math.max(data.idleSeconds * 1.6, data.bucket * 2.5, data.interval * 3);
	const replay = { from: data.from, to: data.to, players: data.players, gap, hold: data.idleSeconds, t: 0, playing: false, trails: null };
	const segments = trailSegments(data.players, gap);
	if (segments.times.length) {
		const geometry = new THREE.BufferGeometry();
		geometry.setAttribute('position', new THREE.BufferAttribute(segments.positions, 3));
		geometry.setAttribute('aTime', new THREE.BufferAttribute(segments.times, 1));
		geometry.setAttribute('aOwner', new THREE.BufferAttribute(Float32Array.from(segments.owners), 1));
		// Lift trails off the ground a little
		geometry.translate(0, 0.4, 0);
		replay.trails = new THREE.LineSegments(geometry, new THREE.ShaderMaterial({
			vertexShader: TRAIL_VERTEX, fragmentShader: TRAIL_FRAGMENT, transparent: true, depthWrite: false,
			uniforms: { uNow: { value: 0 }, uTrail: { value: 300 }, uSelected: { value: -1 } }
		}));
		replay.trails.frustumCulled = false;
		scene.add(replay.trails);
	}
	state.replay = replay;
	const scrub = $('replayScrub');
	scrub.max = String(data.to - data.from);
	scrub.value = '0';
	scrub.disabled = $('replayPlay').disabled = false;
	setReplayTime(0);
	if (data.truncated) toast('That was a lot of movement: only the first part is shown. Pick a shorter range.', 'warning');
	if (!data.players.length) toast('Nobody was recorded here then', 'info');
	replay.playing = data.players.length > 0;
	$('replayPlay').textContent = replay.playing ? 'Pause' : 'Play';
	renderPlayerListNow();
}

function setReplayTime(t) {
	const r = state.replay;
	if (!r) return;
	r.t = Math.max(0, Math.min(r.to - r.from, t));
	$('replayScrub').value = String(Math.round(r.t));
	$('replayTime').textContent = new Date((r.from + r.t) * 1000).toLocaleString() + ' (' + formatSpan(r.t) + ' of ' + formatSpan(r.to - r.from) + ')';
}

if (canHistory) {
	$('replayBar').addEventListener('submit', (e) => { e.preventDefault(); loadReplay(); });
	$('replayRange').addEventListener('change', () => {
		const custom = $('replayRange').value === 'custom';
		$('replayCustom').classList.toggle('d-none', !custom);
		$('replayCustom').classList.toggle('d-inline-flex', custom);
		if (custom && !$('replayFrom').value) {
			const local = (d) => new Date(d.getTime() - d.getTimezoneOffset() * 60000).toISOString().slice(0, 16);
			$('replayTo').value = local(new Date());
			$('replayFrom').value = local(new Date(Date.now() - 3600 * 1000));
		}
	});
	$('replayPlay').addEventListener('click', () => {
		const r = state.replay;
		if (!r) return;
		if (!r.playing && r.t >= r.to - r.from) setReplayTime(0);
		r.playing = !r.playing;
		$('replayPlay').textContent = r.playing ? 'Pause' : 'Play';
	});
	$('replayScrub').addEventListener('input', () => setReplayTime(Number($('replayScrub').value)));
}

if ('BroadcastChannel' in window) {
	new BroadcastChannel('dlu-capture-replay').addEventListener('message', (e) => {
		const r = state.replay, m = e.data || {};
		if (!r || !r.capture || String(m.capture) !== r.capture) return;
		r.playing = false;
		$('replayPlay').textContent = 'Play';
		setReplayTime(Number(m.t) || 0);
	});
}

// ---- heat map timelapse ----

// On a property zone every property is its own instance: show them all together, or one (its clone), as the reports do
async function loadHeatProperties(zone) {
	const select = $('heatProperty');
	const property = ((state.meta && state.meta.propertyZones) || []).includes(Number(zone));
	select.hidden = !property;
	if (!property) return;
	const to = state.meta.today;
	select.innerHTML = '<option value="">All properties</option>';
	const res = await api.get('/api/reports/places?zone=' + zone + '&from=' + Math.max(0, to - 179) + '&to=' + to).catch(() => null);
	if (!res || String(state.zone) !== String(zone)) return;
	select.innerHTML = '<option value="">All properties</option>' + (res.places || []).filter((p) => p.property && String(p.place).includes(':')).map((p) =>
		'<option value="' + esc(String(p.clone)) + '">' + esc(p.name) + ' (' + Number(p.events || 0).toLocaleString() + ')</option>').join('');
}

function dayText(day) { return new Date(day * 86400 * 1000).toISOString().slice(0, 10); }

function clearHeat() {
	disposeGroup(layers.heat);
	state.heat = null;
}

async function loadHeat() {
	const days = Number($('heatDays').value), window = Number($('heatWindow').value);
	const to = state.meta.today, from = to - days + 1;
	const kind = $('heatKind').value;
	setStatus('Loading heat map…');
	// Ask for the days before `from` too when a frame sums several, so the first frames are complete
	const first = Math.max(0, window > 1000 ? 0 : from - window + 1);
	const property = $('heatProperty').hidden ? '' : $('heatProperty').value;
	const data = await api.get('/api/world3d/' + state.zone + '/heatmap?kind=' + kind + '&from=' + Math.max(first, to - 179) + '&to=' + to +
		(property !== '' ? '&clone=' + encodeURIComponent(property) : '')).catch(() => null);
	setStatus('');
	if (!data || data.success === false) { toast((data && data.error) || 'Could not load the heat map', 'danger'); return; }
	clearHeat();
	const built = heatFrames(data.cells, from, to, Math.min(window, 100000));
	const kindName = (state.meta.mapKinds.find((k) => String(k.value) === String(kind)) || {}).name || '';
	state.heat = { ...built, cellSize: data.cellSize, index: built.frames.length - 1, playing: false, mesh: null, visibleCells: [], kindName, window, clock: 0 };
	if (built.cells.length) {
		const size = data.cellSize;
		const geometry = new THREE.PlaneGeometry(size * 0.94, size * 0.94).rotateX(-Math.PI / 2);
		const mesh = new THREE.InstancedMesh(geometry, new THREE.MeshBasicMaterial({ transparent: true, opacity: 0.82, depthWrite: false, side: THREE.DoubleSide }), built.cells.length);
		mesh.setColorAt(0, new THREE.Color());
		mesh.renderOrder = 2;
		layers.heat.add(mesh);
		state.heat.mesh = mesh;
		placeHeat();
	}
	const scrub = $('heatScrub');
	scrub.max = String(Math.max(0, built.frames.length - 1));
	scrub.disabled = $('heatPlay').disabled = !built.frames.length;
	$('heatMax').textContent = Math.round(built.max).toLocaleString();
	showHeatFrame(state.heat.index);
	if (data.truncated) toast('Too many squares: only the first days are shown. Pick fewer days.', 'warning');
	if (!built.cells.length) toast('No ' + kindName.toLowerCase() + ' recorded here in that time', 'info');
}

const heatPosition = new THREE.Vector3(), heatScale = new THREE.Vector3(), heatQuaternion = new THREE.Quaternion();
function placeHeat() {
	const h = state.heat;
	if (!h || !h.mesh) return;
	h.base = h.cells.map(([cx, cz]) => {
		const x = (cx + 0.5) * h.cellSize, z = (cz + 0.5) * h.cellSize;
		return [x, (state.heightAt(x, z) ?? state.groundY) + 0.35, z];
	});
	showHeatFrame(h.index);
}

function showHeatFrame(index) {
	const h = state.heat;
	if (!h || !h.frames.length) { $('heatTime').textContent = ''; return; }
	h.index = Math.max(0, Math.min(h.frames.length - 1, index));
	const frame = h.frames[h.index];
	$('heatScrub').value = String(h.index);
	$('heatTime').textContent = dayText(frame.day) + ' · ' + Math.round(frame.total).toLocaleString();
	if (!h.mesh) return;
	h.visibleCells = [];
	frame.values.forEach((value, i) => {
		const visible = value > 0;
		h.visibleCells[i] = visible;
		heatPosition.set(...h.base[i]);
		heatScale.setScalar(visible ? 1 : 0);
		h.mesh.setMatrixAt(i, matrix.compose(heatPosition, heatQuaternion, heatScale));
		if (visible) h.mesh.setColorAt(i, color.setRGB(...heatColor(heatLevel(value, h.max)), THREE.SRGBColorSpace));
	});
	h.mesh.instanceMatrix.needsUpdate = true;
	h.mesh.instanceColor.needsUpdate = true;
	h.mesh.computeBoundingSphere();
	if (state.selected && state.selected.type === 'heat') renderSelection();
}

if (canHeat) {
	const stops = [0, 0.25, 0.5, 0.75, 1].map((f) => 'rgb(' + heatColor(f).map((c) => Math.round(c * 255)).join(',') + ')');
	$('heatLegend').style.background = 'linear-gradient(90deg,' + stops.join(',') + ')';
	$('heatBar').addEventListener('submit', (e) => { e.preventDefault(); loadHeat(); });
	$('heatPlay').addEventListener('click', () => {
		const h = state.heat;
		if (!h) return;
		if (!h.playing && h.index >= h.frames.length - 1) showHeatFrame(0);
		h.playing = !h.playing;
		$('heatPlay').textContent = h.playing ? 'Pause' : 'Play';
	});
	$('heatScrub').addEventListener('input', () => showHeatFrame(Number($('heatScrub').value)));
}

// ---- modes ----

async function setMode(mode) {
	state.mode = mode;
	$('timeline').classList.toggle('d-none', mode === 'live');
	for (const [id, m] of [['replayBar', 'replay'], ['heatBar', 'heat']]) {
		const bar = $(id);
		if (!bar) continue;
		bar.classList.toggle('d-none', mode !== m);
		bar.classList.toggle('d-flex', mode === m);
	}
	if (mode !== 'replay') stopReplay();
	if (mode !== 'heat') clearHeat();
	if (state.selected && state.selected.type === 'heat' && mode !== 'heat') state.selected = null;
	if (mode === 'replay') await loadReplayInstances();
	else renderInstances();
	renderPlayerListNow();
	renderSelection();
}
document.querySelectorAll('input[name=worldMode]').forEach((input) => input.addEventListener('change', () => { if (input.checked) setMode(input.value); }));

$('zoneSelect').addEventListener('change', () => loadZone(Number($('zoneSelect').value)));
$('instanceSelect').addEventListener('change', () => {
	state.instance = Number($('instanceSelect').value);
	state.live.clear();
	if (state.mode === 'replay' && state.replay) loadReplay();
	api.get('/api/live/players').then((d) => onPositions(d.players)).catch(() => {});
});
$('resetViewBtn').addEventListener('click', () => { state.follow = null; frameAll(); renderPlayerListNow(); });
$('fullscreenBtn').addEventListener('click', () => {
	if (document.fullscreenElement) document.exitFullscreen();
	else $('worldWrap').requestFullscreen().catch(() => {});
});
document.addEventListener('keydown', (e) => {
	if (e.key !== ' ' || e.target.closest('input, select, textarea, button')) return;
	const button = state.mode === 'replay' ? $('replayPlay') : state.mode === 'heat' ? $('heatPlay') : null;
	if (button && !button.disabled) { e.preventDefault(); button.click(); }
});

// ---- main loop ----

const followDelta = new THREE.Vector3();
let lastFrame = performance.now(), lastPanel = 0;
function animate() {
	requestAnimationFrame(animate);
	if (document.hidden) return;
	const now = performance.now();
	const dt = Math.min((now - lastFrame) / 1000, 0.25);
	lastFrame = now;

	const r = state.replay;
	if (r && r.playing) {
		setReplayTime(r.t + dt * Number($('replaySpeed').value));
		if (r.t >= r.to - r.from) { r.playing = false; $('replayPlay').textContent = 'Play'; }
	}
	if (r && r.trails) {
		const u = r.trails.material.uniforms;
		u.uNow.value = r.t;
		u.uTrail.value = Number($('trailLength').value) || 0.0001;
		const selectedIndex = state.selected && state.selected.type === 'player' ? r.players.findIndex((p) => p.id === state.selected.id) : -1;
		u.uSelected.value = selectedIndex;
		r.trails.visible = Number($('trailLength').value) > 0;
	}
	const h = state.heat;
	if (h && h.playing) {
		h.clock += dt;
		if (h.clock > 0.35) {
			h.clock = 0;
			if (h.index >= h.frames.length - 1) { h.playing = false; $('heatPlay').textContent = 'Play'; }
			else showHeatFrame(h.index + 1);
		}
	}

	drawPlayers(currentPlayers(now / 1000));
	if (state.follow) {
		const p = drawnPlayers.find((d) => d.id === state.follow);
		if (p) {
			followDelta.set(p.x, p.y + 1, p.z).sub(controls.target);
			controls.target.add(followDelta);
			camera.position.add(followDelta);
		}
	}
	controls.update();
	fitNearPlane(camera, controls.target);
	scenery.update(dt);
	flairs.update(dt);
	renderer.render(scene, camera);
	updateLabels();
	if (now - lastPanel > 250 && state.selected && state.selected.type === 'player') {
		lastPanel = now;
		const el = $('selectedPosition'), p = drawnPlayers.find((d) => d.id === state.selected.id);
		if (el) el.textContent = p ? positionText(p) : 'Not here right now';
	}
}

// ---- start ----

(async () => {
	try {
		await loadMeta();
	} catch (e) {
		setStatus('Could not load the zone list');
		return;
	}
	if (window.Live) Live.onTopic('player_positions', (e) => { if (state.mode === 'live') onPositions(e.players); });
	// Running worlds and their player counts change; refresh them now and then
	setInterval(() => api.get('/api/world3d/meta').then((m) => { state.meta.worlds = m.worlds; renderInstances(); }).catch(() => {}), 30000);
	animate();
	const params = new URLSearchParams(location.search);
	if (params.get('zone') && $('zoneSelect').querySelector('option[value="' + CSS.escape(params.get('zone')) + '"]')) $('zoneSelect').value = params.get('zone');
	if ($('zoneSelect').value) await loadZone(Number($('zoneSelect').value));
	if (params.get('capture') && canHistory) {
		$('modeReplay').checked = true;
		await setMode('replay');
		await loadCaptureReplay(params.get('capture'));
	} else if (params.get('capture')) {
		toast('Replaying a capture here needs the players_history permission', 'warning');
	}
})();
