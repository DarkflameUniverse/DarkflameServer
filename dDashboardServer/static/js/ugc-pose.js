/**
 * The icon pose editor's 3D view: a player model's .nif or a car or rocket's assembled modules, seen exactly as the
 * UGC server's icon camera sees them (ugc-pose-math.js, the same math as UgcIconPose): the same perspective, framing
 * and model turn, with the icon's square outlined. Dragging changes the icon parameters: the camera around the model,
 * the model's own turn, the sun, or the icon's shift; the wheel changes the border. The light is an approximation (no
 * shadows or highlights); the preview beside it is the UGC server's own drawing.
 */
import * as THREE from 'three';
import { parseModel, mergeMeshes, linearColors } from '/js/scenery-core.js';
import { cameraDirection, computeFrame, modelRotation, viewProjection, wrapDegrees } from '/js/ugc-pose-math.js';

// How much more than the icon's square the view shows around it
const CONTEXT = 1.35;
const DEGREES_PER_PIXEL = 0.4;

export function createPoseEditor(container, options = {}) {
	const renderer = new THREE.WebGLRenderer({ antialias: true, preserveDrawingBuffer: true });
	renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
	renderer.toneMapping = THREE.LinearToneMapping;
	container.textContent = '';
	container.style.position = 'relative';
	container.style.touchAction = 'none';
	container.style.overflow = 'hidden';
	container.appendChild(renderer.domElement);
	renderer.domElement.style.display = 'block';

	// The icon's square
	const overlay = document.createElement('div');
	overlay.style.cssText = 'position:absolute;pointer-events:none;border:1px dashed rgba(255,200,0,.9);box-shadow:0 0 0 9999px rgba(0,0,0,.4)';
	container.appendChild(overlay);

	const scene = new THREE.Scene();
	scene.background = new THREE.Color(0x2a2e35);
	const ambient = new THREE.AmbientLight(0xffffff, 1);
	const sun = new THREE.DirectionalLight(0xffffff, 1);
	const fill = new THREE.DirectionalLight(0xffffff, 0);
	scene.add(ambient, sun, sun.target, fill, fill.target);
	const camera = new THREE.PerspectiveCamera(40, 1, 0.1, 1000);
	camera.matrixAutoUpdate = false;
	const root = new THREE.Group();
	root.matrixAutoUpdate = false;
	scene.add(root);
	const sunArrow = new THREE.ArrowHelper(new THREE.Vector3(0, -1, 0), new THREE.Vector3(), 1, 0xffcc33);
	scene.add(sunArrow);

	let positions = new Float32Array(0);
	let parts = [];
	let values = {};
	let mode = 'camera';
	let frame = null;
	let disposed = false, queued = false;
	let showSun = true;

	function resize() {
		const width = container.clientWidth || 300, height = container.clientHeight || 300;
		renderer.setSize(width, height, false);
		renderer.domElement.style.width = '100%';
		renderer.domElement.style.height = '100%';
		const side = Math.min(width, height) / CONTEXT;
		overlay.style.width = overlay.style.height = side + 'px';
		overlay.style.left = (width - side) / 2 + 'px';
		overlay.style.top = (height - side) / 2 + 'px';
		draw();
	}
	const observer = new ResizeObserver(resize);
	observer.observe(container);

	function v(key, fallback) { return Number.isFinite(values[key]) ? values[key] : fallback; }

	// The renderer's camera and turn for the current values; the scene drawn once
	function update() {
		queued = false;
		if (disposed) return;
		const turn = modelRotation(v('modelYaw', 0), v('modelPitch', 0), v('modelRoll', 0));
		frame = computeFrame(positions, turn, { yaw: v('yaw', 0), pitch: v('pitch', 0), fov: v('fov', 40), margin: v('margin', 1), offsetX: v('offsetX', 0), offsetY: v('offsetY', 0) });
		root.matrix.fromArray(turn);
		root.matrixWorldNeedsUpdate = true;
		const width = container.clientWidth || 300, height = container.clientHeight || 300;
		camera.matrix.fromArray(frame.view).invert();
		camera.matrixWorldNeedsUpdate = true;
		camera.updateMatrixWorld(true);
		if (frame.ok) {
			camera.projectionMatrix.fromArray(viewProjection(frame, width / height, CONTEXT));
			camera.projectionMatrixInverse.copy(camera.projectionMatrix).invert();
		}
		// The light, as close as a simple material gets to the icon's (UgcRender: ambient radiance, irradiances over pi)
		const center = new THREE.Vector3(...frame.center);
		const toSun = new THREE.Vector3(...cameraDirection(v('sunYaw', 21), v('sunPitch', 50)));
		sun.position.copy(center).addScaledVector(toSun, frame.radius * 4);
		sun.target.position.copy(center);
		sun.intensity = v('sunStrength', 2);
		ambient.intensity = v('ambient', 1) * Math.PI;
		fill.position.set(...frame.eye);
		fill.target.position.copy(center);
		fill.intensity = v('fill', 0);
		renderer.toneMappingExposure = v('exposure', 1);
		renderer.domElement.style.filter = 'contrast(' + v('contrast', 1) + ')';
		sunArrow.visible = showSun;
		sunArrow.position.copy(center).addScaledVector(toSun, frame.radius * 1.3);
		sunArrow.setDirection(toSun.clone().negate());
		sunArrow.setLength(frame.radius * 0.6, frame.radius * 0.15, frame.radius * 0.08);
		renderer.render(scene, camera);
	}
	function draw() {
		if (queued) return;
		queued = true;
		requestAnimationFrame(update);
	}

	function clear() {
		for (const part of parts) {
			root.remove(part);
			part.geometry.dispose();
			part.material.dispose();
		}
		parts = [];
		positions = new Float32Array(0);
	}

	// Dragging: the mode's parameters (or by modifier: shift pans, ctrl turns the model, alt moves the sun)
	let drag = null;
	function changed(keys) {
		draw();
		if (options.onChange) options.onChange(Object.fromEntries(keys.map((k) => [k, values[k]])));
	}
	function clamp(key, value) {
		const limits = options.limits && options.limits[key];
		return limits ? Math.min(limits.max, Math.max(limits.min, value)) : value;
	}
	renderer.domElement.addEventListener('contextmenu', (e) => e.preventDefault());
	renderer.domElement.addEventListener('pointerdown', (e) => {
		renderer.domElement.setPointerCapture(e.pointerId);
		const how = e.button === 2 || e.shiftKey ? 'shift' : e.ctrlKey || e.metaKey ? 'model' : e.altKey ? 'sun' : mode;
		drag = { x: e.clientX, y: e.clientY, how };
	});
	renderer.domElement.addEventListener('pointermove', (e) => {
		if (!drag) return;
		const dx = e.clientX - drag.x, dy = e.clientY - drag.y;
		drag.x = e.clientX;
		drag.y = e.clientY;
		if (drag.how === 'camera') {
			values.yaw = wrapDegrees(v('yaw', 0) - dx * DEGREES_PER_PIXEL);
			values.pitch = clamp('pitch', v('pitch', 0) + dy * DEGREES_PER_PIXEL);
			changed(['yaw', 'pitch']);
		} else if (drag.how === 'model') {
			values.modelYaw = wrapDegrees(v('modelYaw', 0) + dx * DEGREES_PER_PIXEL);
			values.modelPitch = clamp('modelPitch', v('modelPitch', 0) + dy * DEGREES_PER_PIXEL);
			changed(['modelYaw', 'modelPitch']);
		} else if (drag.how === 'sun') {
			values.sunYaw = wrapDegrees(v('sunYaw', 0) - dx * DEGREES_PER_PIXEL);
			values.sunPitch = clamp('sunPitch', v('sunPitch', 0) - dy * DEGREES_PER_PIXEL);
			changed(['sunYaw', 'sunPitch']);
		} else {
			const side = Math.min(container.clientWidth, container.clientHeight) / CONTEXT;
			values.offsetX = clamp('offsetX', v('offsetX', 0) + dx / side);
			values.offsetY = clamp('offsetY', v('offsetY', 0) - dy / side);
			changed(['offsetX', 'offsetY']);
		}
	});
	const end = () => { drag = null; };
	renderer.domElement.addEventListener('pointerup', end);
	renderer.domElement.addEventListener('pointercancel', end);
	renderer.domElement.addEventListener('wheel', (e) => {
		e.preventDefault();
		if (e.altKey || mode === 'model' && e.shiftKey) {
			values.modelRoll = wrapDegrees(v('modelRoll', 0) + Math.sign(e.deltaY) * 5);
			changed(['modelRoll']);
			return;
		}
		values.margin = clamp('margin', v('margin', 1) * Math.exp(Math.sign(e.deltaY) * 0.05));
		changed(['margin']);
	}, { passive: false });

	return {
		/** Loads a mesh (the dashboard's converted .nif); resolves {triangles, vertices} */
		async load(url) {
			const response = await fetch(url, { credentials: 'same-origin' });
			if (!response.ok) {
				let message = response.status === 408 ? 'Being made, try again in a moment' : 'HTTP ' + response.status;
				try { message = (await response.json()).error || message; } catch (e) { /* not JSON */ }
				throw new Error(message);
			}
			const model = parseModel(await response.arrayBuffer());
			clear();
			let triangles = 0, vertices = 0;
			const all = [];
			for (const mesh of mergeMeshes(model.meshes)) {
				if (!mesh.vertices || !mesh.indices.length) continue;
				const geometry = new THREE.BufferGeometry();
				geometry.setAttribute('position', new THREE.BufferAttribute(mesh.positions, 3));
				if (mesh.normals) geometry.setAttribute('normal', new THREE.BufferAttribute(mesh.normals, 3, true));
				const hasColors = !!(mesh.colors && mesh.vertexColors !== 0);
				if (hasColors) geometry.setAttribute('color', new THREE.BufferAttribute(linearColors(mesh.colors), 4, true));
				geometry.setIndex(new THREE.BufferAttribute(mesh.indices, 1));
				if (!mesh.normals) geometry.computeVertexNormals();
				let seeThrough = !!mesh.blend && mesh.alpha < 0.99;
				if (mesh.blend && hasColors) for (let i = 3; i < mesh.colors.length && !seeThrough; i += 4) seeThrough = mesh.colors[i] < 250;
				const color = new THREE.Color().setRGB(mesh.diffuse[0], mesh.diffuse[1], mesh.diffuse[2], THREE.SRGBColorSpace);
				const material = new THREE.MeshLambertMaterial({ color, vertexColors: hasColors, transparent: seeThrough, side: THREE.DoubleSide });
				const object = new THREE.Mesh(geometry, material);
				if (seeThrough) object.renderOrder = 1;
				root.add(object);
				parts.push(object);
				all.push(mesh.positions);
				triangles += mesh.indices.length / 3;
				vertices += mesh.vertices;
			}
			positions = new Float32Array(vertices * 3);
			let at = 0;
			for (const list of all) {
				positions.set(list, at);
				at += list.length;
			}
			draw();
			return { triangles, vertices };
		},
		/** The icon parameters (any subset; the rest stay) */
		setValues(next) {
			values = { ...values, ...next };
			draw();
		},
		values() { return { ...values }; },
		/** What a plain drag changes: camera, model, sun or shift */
		setMode(next) { mode = next; },
		setSunArrow(on) { showSun = on; draw(); },
		/** The view as a PNG data URL (for comparing with the icon) */
		snapshot() { update(); return renderer.domElement.toDataURL('image/png'); },
		frame() { return frame; },
		dispose() {
			disposed = true;
			observer.disconnect();
			clear();
			renderer.dispose();
			renderer.domElement.remove();
			overlay.remove();
		}
	};
}
