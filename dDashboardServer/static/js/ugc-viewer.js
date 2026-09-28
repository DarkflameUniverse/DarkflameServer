/**
 * A 3D view of a .nif the UGC server made, from /api/ugc/mesh/:id (the dashboard converts it with NifFile::Encode,
 * as it does the scenery's models), with wireframe and vertex color switches and triangle counts. The metal and glow
 * groups (the UGC server's shader settings; each mesh's "look" is its shader's eShaderLook bits) are drawn as metal
 * reflecting the view's environment and as unlit glow, the glitter groups with moving white flecks (their UVs and
 * uvScroll, as the game moves its fleck texture).
 */
import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
import { RoomEnvironment } from 'three/addons/environments/RoomEnvironment.js';
import { parseModel, mergeMeshes, linearColors, metalOf, addGlitter, SHADER_LOOK } from '/js/scenery-core.js';

export function createNifViewer(container) {
	const renderer = new THREE.WebGLRenderer({ antialias: true });
	renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
	container.textContent = '';
	container.style.position = 'relative';
	container.appendChild(renderer.domElement);

	const scene = new THREE.Scene();
	scene.background = new THREE.Color(0x1e2126);
	const pmrem = new THREE.PMREMGenerator(renderer);
	scene.environment = pmrem.fromScene(new RoomEnvironment(), 0.04).texture;
	scene.environmentIntensity = 0.5;
	const sun = new THREE.DirectionalLight(0xffffff, 1.4);
	scene.add(sun, sun.target);

	const camera = new THREE.PerspectiveCamera(40, 1, 0.05, 20000);
	const controls = new OrbitControls(camera, renderer.domElement);
	controls.enableDamping = true;
	const root = new THREE.Group();
	scene.add(root);

	let parts = [];
	const clock = new THREE.Clock();
	let wireframe = false, vertexColors = true, disposed = false, framed = false;
	const grey = new THREE.Color(0xbbbbbb);

	function resize() {
		const width = container.clientWidth || 300, height = container.clientHeight || 300;
		renderer.setSize(width, height, false);
		renderer.domElement.style.width = '100%';
		renderer.domElement.style.height = '100%';
		camera.aspect = width / height;
		camera.updateProjectionMatrix();
	}
	const observer = new ResizeObserver(resize);
	observer.observe(container);
	resize();

	function loop() {
		if (disposed) return;
		controls.update();
		const seconds = clock.getElapsedTime();
		for (const part of parts) if (part.glitter) part.glitter.update(seconds);
		renderer.render(scene, camera);
		requestAnimationFrame(loop);
	}
	requestAnimationFrame(loop);

	function clear() {
		for (const part of parts) {
			root.remove(part.mesh);
			part.mesh.geometry.dispose();
			part.mesh.material.dispose();
		}
		parts = [];
	}

	function applyLook() {
		for (const part of parts) {
			const material = part.mesh.material;
			material.wireframe = wireframe;
			material.vertexColors = vertexColors && part.hasColors;
			material.color.copy(vertexColors && part.hasColors ? part.baseColor : grey);
			material.needsUpdate = true;
		}
	}

	function frame() {
		const box = new THREE.Box3().setFromObject(root);
		if (box.isEmpty()) return;
		const center = box.getCenter(new THREE.Vector3());
		const radius = Math.max(box.getSize(new THREE.Vector3()).length() / 2, 0.1);
		// From the icon's side: in front and to the side, a little above
		const direction = new THREE.Vector3(Math.sin(0.93) * Math.cos(0.34), Math.sin(0.34), Math.cos(0.93) * Math.cos(0.34));
		camera.position.copy(center).addScaledVector(direction, radius / Math.sin(THREE.MathUtils.degToRad(camera.fov / 2)) * 1.05);
		camera.near = radius / 100;
		camera.far = radius * 100;
		camera.updateProjectionMatrix();
		controls.target.copy(center);
		sun.position.copy(center).add(new THREE.Vector3(0.23, 0.77, 0.6).multiplyScalar(radius * 4));
		sun.target.position.copy(center);
		framed = true;
	}

	return {
		/**
		 * Loads a mesh; resolves {triangles, vertices, shapes}. The view keeps its angle between loads of the same model
		 * (LOD, version) so they can be compared; `reframe` points it at the model again.
		 */
		async load(url, reframe) {
			const response = await fetch(url, { credentials: 'same-origin' });
			if (!response.ok) {
				let message = response.status === 408 ? 'Being made, try again in a moment' : 'HTTP ' + response.status;
				try { message = (await response.json()).error || message; } catch (e) { /* not JSON */ }
				throw new Error(message);
			}
			const model = parseModel(await response.arrayBuffer());
			clear();
			let triangles = 0, vertices = 0;
			for (const mesh of mergeMeshes(model.meshes)) {
				if (!mesh.vertices || !mesh.indices.length) continue;
				const geometry = new THREE.BufferGeometry();
				geometry.setAttribute('position', new THREE.BufferAttribute(mesh.positions, 3));
				if (mesh.normals) geometry.setAttribute('normal', new THREE.BufferAttribute(mesh.normals, 3, true));
				const hasColors = !!(mesh.colors && mesh.vertexColors !== 0);
				if (hasColors) geometry.setAttribute('color', new THREE.BufferAttribute(linearColors(mesh.colors), 4, true));
				geometry.setIndex(new THREE.BufferAttribute(mesh.indices, 1));
				if (!mesh.normals) geometry.computeVertexNormals();
				const baseColor = new THREE.Color().setRGB(mesh.diffuse[0], mesh.diffuse[1], mesh.diffuse[2], THREE.SRGBColorSpace);
				// Blending only matters where something is see-through (every shape of a brick model blends)
				let seeThrough = !!mesh.blend && mesh.alpha < 0.99;
				if (mesh.blend && hasColors) for (let i = 3; i < mesh.colors.length && !seeThrough; i += 4) seeThrough = mesh.colors[i] < 250;
				const look = mesh.look || 0;
				const metal = metalOf(look);
				// Glow: the emissive shader's vertex color, unlit (its vertex alpha is the glow, not opacity)
				const material = look & SHADER_LOOK.EMISSIVE
					? new THREE.MeshBasicMaterial({ color: baseColor.clone(), vertexColors: hasColors })
					: new THREE.MeshStandardMaterial({ color: baseColor.clone(), vertexColors: hasColors, transparent: seeThrough,
						roughness: metal === 'polished' ? 0.18 : metal === 'brushed' ? 0.45 : 0.6, metalness: metal ? 1 : 0 });
				// Glitter: the fleck texture's UVs, one tile of it each, moving as the game moves it
				let glitter = null;
				if (look & SHADER_LOOK.GLITTER && mesh.uvs) {
					geometry.setAttribute('glitterUv', new THREE.BufferAttribute(mesh.uvs, 2));
					glitter = addGlitter(material, { coordinates: 'uv', scroll: mesh.uvScroll || [0, 0] });
				}
				const object = new THREE.Mesh(geometry, material);
				if (seeThrough) object.renderOrder = 1;
				root.add(object);
				parts.push({ mesh: object, hasColors, baseColor, glitter });
				triangles += mesh.indices.length / 3;
				vertices += mesh.vertices;
			}
			applyLook();
			if (reframe || !framed) frame();
			return { triangles, vertices, shapes: model.meshes.length };
		},
		setWireframe(on) { wireframe = on; applyLook(); },
		setVertexColors(on) { vertexColors = on; applyLook(); },
		frame,
		dispose() {
			disposed = true;
			observer.disconnect();
			clear();
			controls.dispose();
			pmrem.dispose();
			renderer.dispose();
			renderer.domElement.remove();
		}
	};
}
