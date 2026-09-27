// The dashboard's pose editor math against the fixture the UGC server's (UgcIconPose) is checked against too, so the
// editor's 3D view and the icons agree. Run by ctest: node ugc-pose-math.test.mjs <ugc-pose-math.js> <fixture.json>
import { readFileSync } from 'node:fs';
import { pathToFileURL } from 'node:url';

const [modulePath, fixturePath] = process.argv.slice(2);
const P = await import(pathToFileURL(modulePath).href);
const fixture = JSON.parse(readFileSync(fixturePath, 'utf8'));
let failures = 0;
const near = (a, b, tolerance, what) => {
	if (!(Math.abs(a - b) <= tolerance)) {
		failures++;
		console.error(`${what}: ${a} is not ${b}`);
	}
};

// Round trips
for (const yaw of [-170, -53, 0, 21, 90, 179]) for (const pitch of [-80, -10, 0, 19.54, 60]) {
	const [y, p] = P.directionAngles(P.cameraDirection(yaw, pitch).map((v) => v * 3));
	near(y, yaw, 1e-6, `camera yaw ${yaw}`);
	near(p, pitch, 1e-6, `camera pitch ${pitch}`);
}
for (const [yaw, pitch, roll] of [[0, 0, 0], [30, 20, 10], [-120, -45, 170], [90, 89, -90], [179, 0, -179]]) {
	const [y, p, r] = P.rotationAngles(P.modelRotation(yaw, pitch, roll));
	near(y, yaw, 1e-4, 'model yaw');
	near(p, pitch, 1e-4, 'model pitch');
	near(r, roll, 1e-4, 'model roll');
}
near(P.wrapDegrees(190), -170, 1e-9, 'wrap');
near(P.wrapDegrees(-181), 179, 1e-9, 'wrap');

// The fixture (the C++ test checks the same numbers)
for (const [i, c] of fixture.cases.entries()) {
	const rotation = P.modelRotation(c.pose.modelYaw, c.pose.modelPitch, c.pose.modelRoll);
	const frame = P.computeFrame(fixture.positions, rotation, c.pose);
	c.center.forEach((v, k) => near(frame.center[k], v, 1e-5, `case ${i} center`));
	c.eye.forEach((v, k) => near(frame.eye[k], v, 1e-4, `case ${i} eye`));
	near(frame.scale, c.scale, 1e-5, `case ${i} scale`);
	c.iconPoints.forEach((point, v) => {
		const got = P.iconPoint(frame, P.transformPoint(rotation, fixture.positions.slice(v * 3, v * 3 + 3)));
		near(got[0], point[0], 1e-5, `case ${i} vertex ${v} x`);
		near(got[1], point[1], 1e-5, `case ${i} vertex ${v} y`);
	});
	// The editor's view: the icon's square is the centred square of 1 / context of the view
	for (const aspect of [1, 1.6, 0.7]) {
		const vp = P.multiply(P.viewProjection(frame, aspect, 1.4), frame.view);
		const rect = P.iconRect(frame);
		// A world point on the icon's top left corner (NDC of the frame's own camera, through its inverse)
		const inv = invert(frame.viewProjection);
		const h = [0, 1, 2, 3].map((r) => inv[r] * rect[0] + inv[4 + r] * rect[3] + inv[8 + r] * 0.5 + inv[12 + r]);
		const corner = [h[0] / h[3], h[1] / h[3], h[2] / h[3]];
		const c4 = [0, 1, 2, 3].map((r) => vp[r] * corner[0] + vp[4 + r] * corner[1] + vp[8 + r] * corner[2] + vp[12 + r]);
		// Lands on the view's centred square of 1 / 1.4 of its shorter side
		near(c4[0] / c4[3], aspect >= 1 ? -1 / (1.4 * aspect) : -1 / 1.4, 1e-4, `case ${i} view corner x (${aspect})`);
		near(c4[1] / c4[3], aspect >= 1 ? 1 / 1.4 : aspect / 1.4, 1e-4, `case ${i} view corner y (${aspect})`);
	}
}

// A general 4x4 inverse (for the check above), with the perspective divide
function invert(m) {
	const inv = new Array(16);
	inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
	inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
	inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
	inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
	inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
	inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
	inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
	inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
	inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
	inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
	inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
	inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
	inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
	inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
	inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
	inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];
	const det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
	return inv.map((v) => v / det);
}

if (failures) {
	console.error(`${failures} check(s) failed`);
	process.exit(1);
}
console.log('ugc-pose-math: all checks passed');
