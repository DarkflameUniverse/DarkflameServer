/**
 * The icon pose's math, as the UGC server works it out (dUgcServer/UgcIconPose.cpp), without three.js so node can test
 * it (tests/dWebTests/ugc-pose-math.test.mjs checks both against the same fixture). Angles are degrees; matrices are
 * column-major arrays of 16, as glm and three.js keep them.
 *
 * The model is turned first (modelRotation, about its origin), then the camera looks at the centre of its bounds from
 * cameraDirection, as far away as makes the bounding sphere fill the field of view, and last the picture is cropped to
 * the model's projected bounds: scaled so the larger side fills the icon less the margin, then shifted.
 */

const RAD = Math.PI / 180;

// From the model towards the camera: yaw around +Y from +Z towards +X, pitch up from the ground
export function cameraDirection(yaw, pitch) {
	const y = yaw * RAD, p = pitch * RAD;
	return [Math.sin(y) * Math.cos(p), Math.sin(p), Math.cos(y) * Math.cos(p)];
}

// The inverse: [yaw, pitch] of a direction
export function directionAngles(d) {
	return [Math.atan2(d[0], d[2]) / RAD, Math.atan2(d[1], Math.hypot(d[0], d[2])) / RAD];
}

export function multiply(a, b) {
	const out = new Array(16).fill(0);
	for (let c = 0; c < 4; c++) for (let r = 0; r < 4; r++) {
		let sum = 0;
		for (let k = 0; k < 4; k++) sum += a[k * 4 + r] * b[c * 4 + k];
		out[c * 4 + r] = sum;
	}
	return out;
}

function rotation(axis, degrees) {
	const c = Math.cos(degrees * RAD), s = Math.sin(degrees * RAD);
	if (axis === 'y') return [c, 0, -s, 0, 0, 1, 0, 0, s, 0, c, 0, 0, 0, 0, 1];
	if (axis === 'x') return [1, 0, 0, 0, 0, c, s, 0, 0, -s, c, 0, 0, 0, 0, 1];
	return [c, s, 0, 0, -s, c, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1];
}

// The model's turn: R = Ry(yaw) * Rx(pitch) * Rz(roll) (three.js's Euler order 'YXZ')
export function modelRotation(yaw, pitch, roll) {
	return multiply(multiply(rotation('y', yaw), rotation('x', pitch)), rotation('z', roll));
}

// The inverse: [yaw, pitch, roll]
export function rotationAngles(m) {
	const m13 = m[8], m23 = m[9], m33 = m[10], m21 = m[1], m22 = m[5], m11 = m[0], m31 = m[2];
	const pitch = Math.asin(Math.max(-1, Math.min(1, -m23)));
	if (Math.abs(m23) < 0.9999999) return [Math.atan2(m13, m33) / RAD, pitch / RAD, Math.atan2(m21, m22) / RAD];
	return [Math.atan2(-m31, m11) / RAD, pitch / RAD, 0];
}

export function transformPoint(m, p) {
	return [m[0] * p[0] + m[4] * p[1] + m[8] * p[2] + m[12], m[1] * p[0] + m[5] * p[1] + m[9] * p[2] + m[13], m[2] * p[0] + m[6] * p[1] + m[10] * p[2] + m[14]];
}

// glm::perspective (right handed, depth -1..1)
export function perspective(fov, aspect, near, far) {
	const f = 1 / Math.tan(fov / 2);
	return [f / aspect, 0, 0, 0, 0, f, 0, 0, 0, 0, -(far + near) / (far - near), -1, 0, 0, -(2 * far * near) / (far - near), 0];
}

// glm::lookAt (right handed)
export function lookAt(eye, center, up) {
	const sub = (a, b) => [a[0] - b[0], a[1] - b[1], a[2] - b[2]];
	const norm = (a) => { const l = Math.hypot(a[0], a[1], a[2]) || 1; return [a[0] / l, a[1] / l, a[2] / l]; };
	const cross = (a, b) => [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]];
	const dot = (a, b) => a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
	const f = norm(sub(center, eye)), s = norm(cross(f, up)), u = cross(s, f);
	return [s[0], u[0], -f[0], 0, s[1], u[1], -f[1], 0, s[2], u[2], -f[2], 0, -dot(s, eye), -dot(u, eye), dot(f, eye), 1];
}

function clip(m, p) {
	return [m[0] * p[0] + m[4] * p[1] + m[8] * p[2] + m[12], m[1] * p[0] + m[5] * p[1] + m[9] * p[2] + m[13],
		m[2] * p[0] + m[6] * p[1] + m[10] * p[2] + m[14], m[3] * p[0] + m[7] * p[1] + m[11] * p[2] + m[15]];
}

/**
 * The frame of a model: `positions` a flat array (x, y, z, ...) of its vertices before the turn, `rotation` the turn
 * (modelRotation), `camera` {yaw, pitch, fov, margin, offsetX, offsetY}. {ok, center, radius, eye, fov (radians),
 * distance, near, far, view, projection, centerX, centerY, scale, offset}
 */
export function computeFrame(positions, rotation, camera) {
	const r = rotation;
	let minX = Infinity, minY = Infinity, minZ = Infinity, maxX = -Infinity, maxY = -Infinity, maxZ = -Infinity;
	const n = positions.length / 3;
	const turned = new Float32Array(positions.length);
	for (let i = 0; i < n; i++) {
		const x = positions[i * 3], y = positions[i * 3 + 1], z = positions[i * 3 + 2];
		const tx = r[0] * x + r[4] * y + r[8] * z + r[12], ty = r[1] * x + r[5] * y + r[9] * z + r[13], tz = r[2] * x + r[6] * y + r[10] * z + r[14];
		turned[i * 3] = tx; turned[i * 3 + 1] = ty; turned[i * 3 + 2] = tz;
		if (tx < minX) minX = tx; if (tx > maxX) maxX = tx;
		if (ty < minY) minY = ty; if (ty > maxY) maxY = ty;
		if (tz < minZ) minZ = tz; if (tz > maxZ) maxZ = tz;
	}
	const frame = { ok: false };
	if (n === 0) {
		frame.center = [0, 0, 0];
		frame.radius = 1;
	} else {
		frame.center = [(minX + maxX) / 2, (minY + maxY) / 2, (minZ + maxZ) / 2];
		frame.radius = Math.max(Math.hypot(maxX - minX, maxY - minY, maxZ - minZ) / 2, 0.01);
	}
	const dir = cameraDirection(camera.yaw, camera.pitch);
	frame.fov = Math.max(1, Math.min(120, camera.fov)) * RAD;
	frame.distance = frame.radius / Math.sin(frame.fov / 2);
	frame.eye = [0, 1, 2].map((k) => frame.center[k] + dir[k] * frame.distance);
	frame.near = Math.max(frame.distance - frame.radius * 1.5, frame.distance * 0.01);
	frame.far = frame.distance + frame.radius * 1.5;
	frame.view = lookAt(frame.eye, frame.center, [0, 1, 0]);
	frame.projection = perspective(frame.fov, 1, frame.near, frame.far);
	frame.viewProjection = multiply(frame.projection, frame.view);
	let pMinX = Infinity, pMinY = Infinity, pMaxX = -Infinity, pMaxY = -Infinity;
	const vp = frame.viewProjection;
	for (let i = 0; i < n; i++) {
		const c = clip(vp, [turned[i * 3], turned[i * 3 + 1], turned[i * 3 + 2]]);
		if (c[3] <= 0) continue;
		const x = c[0] / c[3], y = c[1] / c[3];
		if (x < pMinX) pMinX = x; if (x > pMaxX) pMaxX = x;
		if (y < pMinY) pMinY = y; if (y > pMaxY) pMaxY = y;
	}
	if (pMinX > pMaxX) return frame;
	frame.centerX = (pMinX + pMaxX) / 2;
	frame.centerY = (pMinY + pMaxY) / 2;
	frame.scale = 2 / (Math.max(pMaxX - pMinX, pMaxY - pMinY, 1e-6) * Math.max(camera.margin, 0.1));
	frame.offset = [camera.offsetX || 0, camera.offsetY || 0];
	frame.ok = true;
	return frame;
}

// A (turned) point's place in the icon: [x right, y down] 0..1 across it, and its depth
export function iconPoint(frame, p) {
	const c = clip(frame.viewProjection, p);
	const w = c[3] > 1e-6 ? c[3] : 1e-6;
	return [0.5 + frame.offset[0] + (c[0] / w - frame.centerX) * frame.scale * 0.5, 0.5 - frame.offset[1] - (c[1] / w - frame.centerY) * frame.scale * 0.5, c[2] / w];
}

// The icon's square in the camera's NDC: [minX, minY, maxX, maxY]
export function iconRect(frame) {
	const half = 2 / frame.scale;
	return [frame.centerX + (-0.5 - frame.offset[0]) * half, frame.centerY + (-0.5 - frame.offset[1]) * half,
		frame.centerX + (0.5 - frame.offset[0]) * half, frame.centerY + (0.5 - frame.offset[1]) * half];
}

/**
 * The projection that shows the icon's square (grown by `context` around its centre, so what is just outside shows)
 * on a view of aspect `aspect` (width over height): the frame's projection with the crop in front, column-major.
 * The icon's square is then the centred square of min(width, height) / context pixels.
 */
export function viewProjection(frame, aspect, context) {
	const rect = iconRect(frame);
	const qx = (rect[0] + rect[2]) / 2, qy = (rect[1] + rect[3]) / 2, half = (rect[2] - rect[0]) / 2 * context;
	const hx = aspect >= 1 ? half * aspect : half, hy = aspect >= 1 ? half : half / aspect;
	// In NDC x' = (x - qx) / hx; in clip space (homogeneous) x' = (x - qx w) / hx, row by row of the projection
	const p = frame.projection;
	const out = p.slice();
	for (let c = 0; c < 4; c++) {
		out[c * 4] = (p[c * 4] - qx * p[c * 4 + 3]) / hx;
		out[c * 4 + 1] = (p[c * 4 + 1] - qy * p[c * 4 + 3]) / hy;
	}
	return out;
}

// Keeps an angle in -180..180
export function wrapDegrees(a) {
	return ((((a + 180) % 360) + 360) % 360) - 180;
}
