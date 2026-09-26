/**
 * The scenery loader's decoding, without three.js so it can be tested with node: the server's converted models
 * (NifFile::Encode) and the client's DDS textures (DXT1/3/5 and uncompressed), which the browser decodes itself.
 */

/**
 * A model from /api/scenery/:zone/mesh/:asset: {header, meshes: [{...header entry, positions, normals, uvs, colors,
 * indices}]} with typed arrays viewing the response (normals int8 x3, colors uint8 RGBA, indices uint16).
 */
export function parseModel(buffer) {
	const view = new DataView(buffer);
	const length = view.getUint32(0, true);
	const header = JSON.parse(new TextDecoder().decode(new Uint8Array(buffer, 4, length)));
	const base = 4 + length;
	const meshes = header.meshes.map((entry) => {
		let offset = base + entry.offset;
		const n = entry.vertices;
		const mesh = { ...entry };
		mesh.positions = new Float32Array(buffer, offset, n * 3);
		offset += n * 12;
		if (entry.normals) {
			mesh.normals = new Int8Array(buffer, offset, n * 3);
			offset += (n * 3 + 3) & ~3;
		}
		if (entry.uv) {
			mesh.uvs = new Float32Array(buffer, offset, n * 2);
			offset += n * 8;
		}
		if (entry.colors) {
			mesh.colors = new Uint8Array(buffer, offset, n * 4);
			offset += n * 4;
		}
		mesh.indices = new Uint16Array(buffer, offset, entry.indices);
		return mesh;
	});
	return { header, meshes };
}

/**
 * Meshes of a model that look the same (texture, colors, blending, sides, attributes) joined into one, so a model
 * made of many pieces (the zones' "glom" files have over a hundred) costs a few draw calls instead of one per piece.
 * Order is kept otherwise; indices become 32-bit when a joined mesh passes 65535 vertices.
 */
export function mergeMeshes(meshes) {
	const groups = new Map();
	for (const mesh of meshes) {
		if (!mesh.vertices || !mesh.indices.length) continue;
		const key = JSON.stringify([mesh.texture, mesh.diffuse, mesh.emissive, mesh.alpha, mesh.blend, mesh.test, mesh.doubleSided,
			mesh.vertexColors, mesh.clampU, mesh.clampV, !!mesh.normals, !!mesh.uvs, !!mesh.colors]);
		if (!groups.has(key)) groups.set(key, []);
		groups.get(key).push(mesh);
	}
	return [...groups.values()].map((list) => {
		if (list.length === 1) return list[0];
		const first = list[0];
		const vertices = list.reduce((sum, m) => sum + m.vertices, 0);
		const indexCount = list.reduce((sum, m) => sum + m.indices.length, 0);
		const out = { ...first, vertices, positions: new Float32Array(vertices * 3) };
		if (first.normals) out.normals = new Int8Array(vertices * 3);
		if (first.uvs) out.uvs = new Float32Array(vertices * 2);
		if (first.colors) out.colors = new Uint8Array(vertices * 4);
		out.indices = vertices > 65535 ? new Uint32Array(indexCount) : new Uint16Array(indexCount);
		let v = 0, i = 0;
		for (const m of list) {
			out.positions.set(m.positions, v * 3);
			if (out.normals) out.normals.set(m.normals, v * 3);
			if (out.uvs) out.uvs.set(m.uvs, v * 2);
			if (out.colors) out.colors.set(m.colors, v * 4);
			for (let k = 0; k < m.indices.length; k++) out.indices[i + k] = m.indices[k] + v;
			v += m.vertices;
			i += m.indices.length;
		}
		return out;
	});
}

// sRGB byte -> linear byte, for vertex colors (three.js takes vertex colors as linear)
const SRGB_TO_LINEAR = new Uint8Array(256);
for (let i = 0; i < 256; i++) {
	const c = i / 255;
	SRGB_TO_LINEAR[i] = Math.round((c <= 0.04045 ? c / 12.92 : Math.pow((c + 0.055) / 1.055, 2.4)) * 255);
}
export function linearColors(srgb) {
	const out = new Uint8Array(srgb.length);
	for (let i = 0; i < srgb.length; i += 4) {
		out[i] = SRGB_TO_LINEAR[srgb[i]];
		out[i + 1] = SRGB_TO_LINEAR[srgb[i + 1]];
		out[i + 2] = SRGB_TO_LINEAR[srgb[i + 2]];
		out[i + 3] = srgb[i + 3];
	}
	return out;
}

const FOURCC = { 0x31545844: 'DXT1', 0x33545844: 'DXT3', 0x35545844: 'DXT5' };

/**
 * A DDS file's header and mipmap levels: {format: 'DXT1'|'DXT3'|'DXT5'|'RGBA', width, height, levels: [{width,
 * height, data}]}, starting at the first level no larger than maxSize (so big textures cost less), or null when the
 * file isn't a DDS this reads. Uncompressed files (24 or 32 bits with masks) come back as RGBA, one level.
 */
export function parseDds(buffer, maxSize = 4096) {
	if (buffer.byteLength < 128) return null;
	const view = new DataView(buffer);
	if (view.getUint32(0, true) !== 0x20534444) return null; // "DDS "
	const height = view.getUint32(12, true), width = view.getUint32(16, true);
	const mipCount = Math.max(1, view.getUint32(28, true));
	const pfFlags = view.getUint32(80, true);
	const fourCC = view.getUint32(84, true);
	if (!width || !height || width > 8192 || height > 8192) return null;
	let offset = 128;
	if (pfFlags & 0x4) {
		const format = FOURCC[fourCC];
		if (!format) return null;
		const blockBytes = format === 'DXT1' ? 8 : 16;
		const levels = [];
		let w = width, h = height;
		for (let i = 0; i < mipCount; i++) {
			const size = Math.max(1, (w + 3) >> 2) * Math.max(1, (h + 3) >> 2) * blockBytes;
			if (offset + size > buffer.byteLength) break;
			levels.push({ width: w, height: h, data: new Uint8Array(buffer, offset, size) });
			offset += size;
			w = Math.max(1, w >> 1);
			h = Math.max(1, h >> 1);
		}
		let first = 0;
		while (first < levels.length - 1 && (levels[first].width > maxSize || levels[first].height > maxSize)) first++;
		const chosen = levels.slice(first);
		if (!chosen.length) return null;
		return { format, width: chosen[0].width, height: chosen[0].height, levels: chosen };
	}
	if (!(pfFlags & 0x40)) return null;
	const bits = view.getUint32(88, true);
	if (bits !== 32 && bits !== 24) return null;
	const masks = [92, 96, 100, 104].map((at) => view.getUint32(at, true));
	const hasAlpha = (pfFlags & 0x1) && masks[3];
	const bytes = bits / 8;
	if (offset + width * height * bytes > buffer.byteLength) return null;
	const shift = (mask) => (mask ? 31 - Math.clz32(mask & -mask) : 0);
	const shifts = masks.map(shift);
	const rgba = new Uint8Array(width * height * 4);
	const src = new Uint8Array(buffer, offset, width * height * bytes);
	for (let i = 0, s = 0; i < width * height; i++, s += bytes) {
		const pixel = bytes === 4 ? (src[s] | (src[s + 1] << 8) | (src[s + 2] << 16) | (src[s + 3] << 24)) >>> 0 : src[s] | (src[s + 1] << 8) | (src[s + 2] << 16);
		rgba[i * 4] = (pixel & masks[0]) >>> shifts[0];
		rgba[i * 4 + 1] = (pixel & masks[1]) >>> shifts[1];
		rgba[i * 4 + 2] = (pixel & masks[2]) >>> shifts[2];
		rgba[i * 4 + 3] = hasAlpha ? (pixel & masks[3]) >>> shifts[3] : 255;
	}
	const image = downscale({ format: 'RGBA', width, height, levels: [{ width, height, data: rgba }] }, maxSize);
	image.alpha = !!hasAlpha;
	return image;
}

// Halve an RGBA image (box filter) until it fits maxSize
function downscale(image, maxSize) {
	let { width, height, data } = image.levels[0];
	while (width > maxSize || height > maxSize) {
		const w = Math.max(1, width >> 1), h = Math.max(1, height >> 1);
		const out = new Uint8Array(w * h * 4);
		for (let y = 0; y < h; y++) {
			for (let x = 0; x < w; x++) {
				for (let c = 0; c < 4; c++) {
					const at = (dx, dy) => data[((Math.min(y * 2 + dy, height - 1)) * width + Math.min(x * 2 + dx, width - 1)) * 4 + c];
					out[(y * w + x) * 4 + c] = (at(0, 0) + at(1, 0) + at(0, 1) + at(1, 1) + 2) >> 2;
				}
			}
		}
		width = w; height = h; data = out;
	}
	return { format: 'RGBA', width, height, levels: [{ width, height, data }] };
}

function color565(value, out, at) {
	out[at] = ((value >> 11) & 31) * 255 / 31 | 0;
	out[at + 1] = ((value >> 5) & 63) * 255 / 63 | 0;
	out[at + 2] = (value & 31) * 255 / 31 | 0;
}

/**
 * One DXT level as RGBA bytes (for GPUs without S3TC support, e.g. most phones). DXT1 blocks whose first color is
 * not greater than the second have a transparent fourth color.
 */
export function decodeDxt(format, width, height, data) {
	const out = new Uint8Array(width * height * 4);
	const blockBytes = format === 'DXT1' ? 8 : 16;
	const bw = Math.max(1, (width + 3) >> 2), bh = Math.max(1, (height + 3) >> 2);
	const palette = new Uint8Array(16);
	const alphas = new Uint8Array(16);
	for (let by = 0; by < bh; by++) {
		for (let bx = 0; bx < bw; bx++) {
			const block = (by * bw + bx) * blockBytes;
			const colorAt = format === 'DXT1' ? block : block + 8;
			const c0 = data[colorAt] | (data[colorAt + 1] << 8), c1 = data[colorAt + 2] | (data[colorAt + 3] << 8);
			color565(c0, palette, 0);
			color565(c1, palette, 4);
			palette[3] = palette[7] = palette[11] = palette[15] = 255;
			if (c0 > c1 || format !== 'DXT1') {
				for (let c = 0; c < 3; c++) {
					palette[8 + c] = (2 * palette[c] + palette[4 + c]) / 3 | 0;
					palette[12 + c] = (palette[c] + 2 * palette[4 + c]) / 3 | 0;
				}
			} else {
				for (let c = 0; c < 3; c++) palette[8 + c] = (palette[c] + palette[4 + c]) >> 1;
				palette[12] = palette[13] = palette[14] = palette[15] = 0;
			}
			if (format === 'DXT3') {
				for (let i = 0; i < 16; i++) alphas[i] = ((data[block + (i >> 1)] >> ((i & 1) * 4)) & 15) * 17;
			} else if (format === 'DXT5') {
				const a0 = data[block], a1 = data[block + 1];
				const table = [a0, a1];
				if (a0 > a1) for (let i = 1; i < 7; i++) table.push(((7 - i) * a0 + i * a1) / 7 | 0);
				else { for (let i = 1; i < 5; i++) table.push(((5 - i) * a0 + i * a1) / 5 | 0); table.push(0, 255); }
				// 48 bits of 3-bit indices
				let bits = 0, count = 0, byte = block + 2;
				for (let i = 0; i < 16; i++) {
					if (count < 3) { bits |= data[byte++] << count; count += 8; }
					alphas[i] = table[bits & 7];
					bits >>= 3; count -= 3;
				}
			}
			const indices = data[colorAt + 4] | (data[colorAt + 5] << 8) | (data[colorAt + 6] << 16) | (data[colorAt + 7] << 24);
			for (let i = 0; i < 16; i++) {
				const x = bx * 4 + (i & 3), y = by * 4 + (i >> 2);
				if (x >= width || y >= height) continue;
				const p = ((indices >>> (i * 2)) & 3) * 4;
				const at = (y * width + x) * 4;
				out[at] = palette[p];
				out[at + 1] = palette[p + 1];
				out[at + 2] = palette[p + 2];
				out[at + 3] = format === 'DXT1' ? palette[p + 3] : alphas[i];
			}
		}
	}
	return out;
}

/** A level chain is complete (usable for mipmapping) when it runs down to 1x1. */
export function completeChain(levels) {
	const last = levels[levels.length - 1];
	return levels.length > 0 && last.width === 1 && last.height === 1;
}

/**
 * Objects of a manifest grouped by model: Map asset -> [{index, x, y, z, qx, qy, qz, qw, scale}].
 */
export function groupObjects(objects) {
	const byAsset = new Map();
	for (let i = 0; i < objects.asset.length; i++) {
		const asset = objects.asset[i];
		if (!byAsset.has(asset)) byAsset.set(asset, []);
		byAsset.get(asset).push({
			index: i, x: objects.pos[i * 3], y: objects.pos[i * 3 + 1], z: objects.pos[i * 3 + 2],
			qx: objects.rot[i * 4], qy: objects.rot[i * 4 + 1], qz: objects.rot[i * 4 + 2], qw: objects.rot[i * 4 + 3], scale: objects.scale[i],
			// A tint per object ([r, g, b, ...]), for the flairs
			color: objects.color ? [objects.color[i * 3], objects.color[i * 3 + 1], objects.color[i * 3 + 2]] : null,
			// The game doesn't draw it (a trigger or blocking volume): only shown on request
			hidden: !!(objects.hidden && objects.hidden[i])
		});
	}
	return byAsset;
}

/** Split instances into square cells `size` wide (by x and z): Map "cx,cz" -> instances. */
export function cellsOf(instances, size) {
	const cells = new Map();
	for (const instance of instances) {
		const key = Math.floor(instance.x / size) + ',' + Math.floor(instance.z / size);
		if (!cells.has(key)) cells.set(key, []);
		cells.get(key).push(instance);
	}
	return cells;
}
