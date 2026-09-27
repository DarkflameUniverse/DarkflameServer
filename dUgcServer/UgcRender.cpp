#include "UgcRender.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

#include <glm/gtc/matrix_transform.hpp>

namespace {
	constexpr float INF = std::numeric_limits<float>::infinity();

	float Edge(const glm::vec3& a, const glm::vec3& b, float px, float py) {
		return (b.x - a.x) * (py - a.y) - (b.y - a.y) * (px - a.x);
	}

	// Calls fragment(x, y, z, w0, w1, w2) for every pixel centre inside the screen-space triangle (x, y in pixels)
	template<typename Fragment>
	void Rasterize(int width, int height, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, Fragment&& fragment) {
		const float area = Edge(a, b, c.x, c.y);
		if (!(std::abs(area) > 1e-9f)) return;
		const float inverse = 1.0f / area;
		const int minX = std::max(0, static_cast<int>(std::floor(std::min({ a.x, b.x, c.x }))));
		const int maxX = std::min(width - 1, static_cast<int>(std::ceil(std::max({ a.x, b.x, c.x }))));
		const int minY = std::max(0, static_cast<int>(std::floor(std::min({ a.y, b.y, c.y }))));
		const int maxY = std::min(height - 1, static_cast<int>(std::ceil(std::max({ a.y, b.y, c.y }))));
		for (int y = minY; y <= maxY; y++) {
			const float py = y + 0.5f;
			for (int x = minX; x <= maxX; x++) {
				const float px = x + 0.5f;
				const float w0 = Edge(b, c, px, py) * inverse;
				const float w1 = Edge(c, a, px, py) * inverse;
				const float w2 = 1.0f - w0 - w1;
				if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) continue;
				fragment(x, y, w0 * a.z + w1 * b.z + w2 * c.z, w0, w1, w2);
			}
		}
	}

	float ToLinear(float c) { return std::pow(std::clamp(c, 0.0f, 1.0f), 2.2f); }
	float ToSrgb(float c) { return std::pow(std::clamp(c, 0.0f, 1.0f), 1.0f / 2.2f); }

	// Looking at a sphere (center, radius) from direction `dir` (towards the viewer), square orthographic view
	struct OrthoView {
		glm::vec3 center{};
		glm::vec3 dir{};
		glm::vec3 right{};
		glm::vec3 up{};
		float radius{};
		int resolution{};

		OrthoView(const glm::vec3& center_, float radius_, const glm::vec3& dir_, int resolution_)
			: center(center_), dir(dir_), radius(radius_), resolution(resolution_) {
			const glm::vec3 worldUp = std::abs(dir.y) < 0.99f ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
			right = glm::normalize(glm::cross(worldUp, dir));
			up = glm::cross(dir, right);
		}

		// x, y in pixels; z the distance from the camera plane (smaller is nearer)
		glm::vec3 Project(const glm::vec3& p) const {
			const auto offset = p - center;
			const float scale = 0.5f * resolution / radius;
			return { resolution * 0.5f + glm::dot(offset, right) * scale, resolution * 0.5f - glm::dot(offset, up) * scale, radius - glm::dot(offset, dir) };
		}

		float PixelSize() const { return 2.0f * radius / resolution; }
	};

	void Bounds(const UgcModel::Model& model, glm::vec3& center, float& radius) {
		glm::vec3 min{}, max{};
		if (!model.Bounds(min, max)) {
			center = glm::vec3(0.0f);
			radius = 1.0f;
			return;
		}
		center = (min + max) * 0.5f;
		radius = std::max(glm::length(max - min) * 0.5f, 0.01f);
	}
}

namespace UgcRender {
	std::vector<glm::vec3> SphereDirections() {
		const float t = (1.0f + std::sqrt(5.0f)) / 2.0f;
		const std::vector<glm::vec3> corners = {
			{ -1, t, 0 }, { 1, t, 0 }, { -1, -t, 0 }, { 1, -t, 0 },
			{ 0, -1, t }, { 0, 1, t }, { 0, -1, -t }, { 0, 1, -t },
			{ t, 0, -1 }, { t, 0, 1 }, { -t, 0, -1 }, { -t, 0, 1 },
		};
		std::vector<glm::vec3> directions;
		for (const auto& corner : corners) directions.push_back(glm::normalize(corner));
		// Edge centres: the pairs of corners that are neighbours (the shortest distance apart)
		const float edge = glm::length(corners[0] - corners[1]);
		for (size_t i = 0; i < corners.size(); i++) {
			for (size_t j = i + 1; j < corners.size(); j++) {
				if (std::abs(glm::length(corners[i] - corners[j]) - edge) < 1e-3f) directions.push_back(glm::normalize(corners[i] + corners[j]));
			}
		}
		return directions;
	}

	OptimizeResult Optimize(UgcModel::Model& model, const OptimizeOptions& options) {
		OptimizeResult result;
		auto& opaque = model.opaque;
		result.trianglesBefore = opaque.TriangleCount() + model.transparent.TriangleCount();
		if (model.Empty() || (!options.removeHidden && !options.bakeAo)) return result;

		glm::vec3 center{};
		float radius{};
		Bounds(model, center, radius);
		radius *= 1.02f;
		const int resolution = std::clamp(options.resolution, 64, 4096);
		const size_t pixels = static_cast<size_t>(resolution) * resolution;
		std::vector<float> depth(pixels);
		std::vector<uint32_t> ids(pixels);
		std::vector<glm::vec3> screen(opaque.positions.size());

		const size_t triangles = opaque.TriangleCount();
		std::vector<bool> visible(triangles, false);
		std::vector<bool> facing(triangles, true);
		std::vector<glm::vec3> faceNormals(triangles, glm::vec3(0.0f));
		for (size_t t = 0; t < triangles; t++) {
			if (opaque.normals.size() != opaque.positions.size()) break;
			faceNormals[t] = glm::normalize(opaque.normals[opaque.indices[t * 3]] + opaque.normals[opaque.indices[t * 3 + 1]] + opaque.normals[opaque.indices[t * 3 + 2]] + glm::vec3(1e-6f));
		}
		// Without normals nothing is culled
		if (opaque.normals.size() != opaque.positions.size()) std::fill(faceNormals.begin(), faceNormals.end(), glm::vec3(0.0f));
		std::vector<float> aoOpaque(opaque.positions.size()), weightOpaque(opaque.positions.size());
		std::vector<float> aoTransparent(model.transparent.positions.size()), weightTransparent(model.transparent.positions.size());

		for (const auto& direction : SphereDirections()) {
			const OrthoView view(center, radius, direction, resolution);
			const float bias = view.PixelSize();
			std::fill(depth.begin(), depth.end(), INF);
			std::fill(ids.begin(), ids.end(), 0);
			for (size_t v = 0; v < opaque.positions.size(); v++) screen[v] = view.Project(opaque.positions[v]);
			// Faces turned away can't be seen from here (they're seen from the directions they face); their vertex
			// normals say which way they face, which doesn't depend on the files' winding
			for (size_t t = 0; t < triangles; t++) facing[t] = glm::dot(faceNormals[t], direction) > -0.1f;
			for (size_t t = 0; t < triangles; t++) {
				if (!facing[t]) continue;
				const auto id = static_cast<uint32_t>(t + 1);
				Rasterize(resolution, resolution, screen[opaque.indices[t * 3]], screen[opaque.indices[t * 3 + 1]], screen[opaque.indices[t * 3 + 2]],
					[&](int x, int y, float z, float, float, float) {
						auto& stored = depth[static_cast<size_t>(y) * resolution + x];
						if (z < stored) {
							stored = z;
							ids[static_cast<size_t>(y) * resolution + x] = id;
						}
					});
			}
			for (const auto id : ids) {
				if (id != 0) visible[id - 1] = true;
			}

			// Whether a point is in front of what was drawn around its pixel
			const auto exposed = [&](const glm::vec3& point, float allowance, bool neighbours) {
				const auto p = view.Project(point);
				const int px = static_cast<int>(std::floor(p.x)), py = static_cast<int>(std::floor(p.y));
				const int reach = neighbours ? 1 : 0;
				for (int dy = -reach; dy <= reach; dy++) {
					for (int dx = -reach; dx <= reach; dx++) {
						const int x = px + dx, y = py + dy;
						if (x < 0 || y < 0 || x >= resolution || y >= resolution) return true;
						if (p.z <= depth[static_cast<size_t>(y) * resolution + x] + allowance) return true;
					}
				}
				return false;
			};

			// Triangles too small or thin to cover a pixel centre: kept when their centre isn't behind what was drawn.
			// Bigger ones that show would have covered one.
			if (options.removeHidden) {
				const float smallArea = 2.0f; // pixels
				for (size_t t = 0; t < triangles; t++) {
					if (visible[t] || !facing[t]) continue;
					const auto& a = screen[opaque.indices[t * 3]];
					const auto& b = screen[opaque.indices[t * 3 + 1]];
					const auto& c = screen[opaque.indices[t * 3 + 2]];
					if (std::abs(Edge(a, b, c.x, c.y)) * 0.5f > smallArea) continue;
					const auto centre = (a + b + c) / 3.0f;
					const int x = static_cast<int>(centre.x), y = static_cast<int>(centre.y);
					if (x < 0 || y < 0 || x >= resolution || y >= resolution || centre.z <= depth[static_cast<size_t>(y) * resolution + x] + bias) visible[t] = true;
				}
			}

			if (options.bakeAo) {
				const auto accumulate = [&](const UgcModel::Mesh& mesh, std::vector<float>& ao, std::vector<float>& weight) {
					for (size_t v = 0; v < mesh.positions.size(); v++) {
						const float w = v < mesh.normals.size() ? glm::dot(mesh.normals[v], direction) : 1.0f;
						if (w <= 0.0f) continue;
						weight[v] += w;
						const auto normal = v < mesh.normals.size() ? mesh.normals[v] : glm::vec3(0.0f);
						if (exposed(mesh.positions[v] + normal * bias, bias, false)) ao[v] += w;
					}
				};
				accumulate(opaque, aoOpaque, weightOpaque);
				accumulate(model.transparent, aoTransparent, weightTransparent);
			}
		}

		if (options.bakeAo) {
			const float strength = std::clamp(options.aoStrength, 0.0f, 1.0f);
			const auto apply = [strength](UgcModel::Mesh& mesh, const std::vector<float>& ao, const std::vector<float>& weight) {
				for (size_t v = 0; v < mesh.colors.size() && v < ao.size(); v++) {
					if (weight[v] <= 0.0f) continue;
					const float factor = 1.0f - strength * (1.0f - ao[v] / weight[v]);
					auto& color = mesh.colors[v];
					color.r = ToSrgb(ToLinear(color.r) * factor);
					color.g = ToSrgb(ToLinear(color.g) * factor);
					color.b = ToSrgb(ToLinear(color.b) * factor);
				}
			};
			apply(opaque, aoOpaque, weightOpaque);
			apply(model.transparent, aoTransparent, weightTransparent);
		}

		if (options.removeHidden) {
			UgcModel::Mesh kept;
			std::vector<uint32_t> remap(opaque.positions.size(), UINT32_MAX);
			for (size_t t = 0; t < triangles; t++) {
				if (!visible[t]) {
					result.trianglesRemoved++;
					continue;
				}
				for (int k = 0; k < 3; k++) {
					const auto source = opaque.indices[t * 3 + k];
					if (remap[source] == UINT32_MAX) {
						remap[source] = static_cast<uint32_t>(kept.positions.size());
						kept.positions.push_back(opaque.positions[source]);
						if (source < opaque.normals.size()) kept.normals.push_back(opaque.normals[source]);
						if (source < opaque.colors.size()) kept.colors.push_back(opaque.colors[source]);
					}
					kept.indices.push_back(remap[source]);
				}
			}
			opaque = std::move(kept);
		}
		return result;
	}

	Image RenderIcon(const UgcModel::Model& source, const IconOptions& options) {
		const int size = std::clamp(options.size, 8, 1024);
		const int supersample = std::clamp(options.supersample, 1, 8);
		const int n = size * supersample;
		Image image{ size, size, std::vector<uint8_t>(static_cast<size_t>(size) * size * 4, 0) };
		if (source.Empty()) return image;

		UgcModel::Model model = source;
		model.opaque.Transform(options.modelRotation);
		model.transparent.Transform(options.modelRotation);
		glm::vec3 center{};
		float radius{};
		Bounds(model, center, radius);

		const float yaw = glm::radians(options.yawDegrees), pitch = glm::radians(options.pitchDegrees);
		const glm::vec3 dir(std::sin(yaw) * std::cos(pitch), std::sin(pitch), std::cos(yaw) * std::cos(pitch));
		const float fov = glm::radians(std::clamp(options.fovDegrees, 1.0f, 120.0f));
		const float distance = radius / std::sin(fov * 0.5f);
		const glm::vec3 eye = center + dir * distance;
		const glm::mat4 viewProjection = glm::perspective(fov, 1.0f, std::max(distance - radius * 1.5f, distance * 0.01f), distance + radius * 1.5f) *
			glm::lookAt(eye, center, glm::vec3(0.0f, 1.0f, 0.0f));

		// Frame the model: its projected bounds, scaled to fill the icon less the margin
		float minX = INF, minY = INF, maxX = -INF, maxY = -INF;
		for (const auto* mesh : { &model.opaque, &model.transparent }) {
			for (const auto& position : mesh->positions) {
				const auto clip = viewProjection * glm::vec4(position, 1.0f);
				if (clip.w <= 0.0f) continue;
				minX = std::min(minX, clip.x / clip.w);
				maxX = std::max(maxX, clip.x / clip.w);
				minY = std::min(minY, clip.y / clip.w);
				maxY = std::max(maxY, clip.y / clip.w);
			}
		}
		if (minX > maxX) return image;
		const float centerX = (minX + maxX) * 0.5f, centerY = (minY + maxY) * 0.5f;
		const float scale = 2.0f / (std::max({ maxX - minX, maxY - minY, 1e-6f }) * std::max(options.margin, 0.1f));
		const auto project = [&](const glm::vec3& position) {
			const auto clip = viewProjection * glm::vec4(position, 1.0f);
			const float w = clip.w > 1e-6f ? clip.w : 1e-6f;
			return glm::vec3((0.5f + (clip.x / w - centerX) * scale * 0.5f) * n, (0.5f - (clip.y / w - centerY) * scale * 0.5f) * n, clip.z / w);
		};

		// Linear, premultiplied
		std::vector<glm::vec4> color(static_cast<size_t>(n) * n, glm::vec4(0.0f));
		std::vector<float> depth(static_cast<size_t>(n) * n, INF);
		const glm::vec3 light = glm::normalize(dir + glm::vec3(-0.35f, 1.1f, 0.25f));
		const auto shade = [&](const UgcModel::Mesh& mesh, uint32_t i0, uint32_t i1, uint32_t i2, float w0, float w1, float w2) {
			glm::vec3 normal(0.0f, 1.0f, 0.0f);
			if (mesh.normals.size() == mesh.positions.size()) {
				normal = mesh.normals[i0] * w0 + mesh.normals[i1] * w1 + mesh.normals[i2] * w2;
				const auto length = glm::length(normal);
				normal = length > 0.0f ? normal / length : glm::vec3(0.0f, 1.0f, 0.0f);
				if (glm::dot(normal, dir) < 0.0f) normal = -normal; // the back of a face
			}
			glm::vec4 base(0.63f, 0.63f, 0.63f, 1.0f);
			if (mesh.colors.size() == mesh.positions.size()) base = mesh.colors[i0] * w0 + mesh.colors[i1] * w1 + mesh.colors[i2] * w2;
			const float lighting = 0.55f + 0.6f * std::max(0.0f, glm::dot(normal, light));
			return glm::vec4(ToLinear(base.r) * lighting, ToLinear(base.g) * lighting, ToLinear(base.b) * lighting, std::clamp(base.a, 0.0f, 1.0f));
		};

		// Opaque first, with the depth buffer
		{
			const auto& mesh = model.opaque;
			std::vector<glm::vec3> screen(mesh.positions.size());
			for (size_t v = 0; v < mesh.positions.size(); v++) screen[v] = project(mesh.positions[v]);
			for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
				const auto i0 = mesh.indices[i], i1 = mesh.indices[i + 1], i2 = mesh.indices[i + 2];
				Rasterize(n, n, screen[i0], screen[i1], screen[i2], [&](int x, int y, float z, float w0, float w1, float w2) {
					const size_t index = static_cast<size_t>(y) * n + x;
					if (z >= depth[index]) return;
					depth[index] = z;
					const auto shaded = shade(mesh, i0, i1, i2, w0, w1, w2);
					color[index] = glm::vec4(glm::vec3(shaded), 1.0f);
				});
			}
		}

		// Then transparent triangles, farthest first, blended over it
		{
			const auto& mesh = model.transparent;
			std::vector<glm::vec3> screen(mesh.positions.size());
			for (size_t v = 0; v < mesh.positions.size(); v++) screen[v] = project(mesh.positions[v]);
			std::vector<size_t> order(mesh.indices.size() / 3);
			std::iota(order.begin(), order.end(), 0);
			const auto depthOf = [&](size_t t) { return screen[mesh.indices[t * 3]].z + screen[mesh.indices[t * 3 + 1]].z + screen[mesh.indices[t * 3 + 2]].z; };
			std::sort(order.begin(), order.end(), [&](size_t a, size_t b) { return depthOf(a) > depthOf(b); });
			for (const auto t : order) {
				const auto i0 = mesh.indices[t * 3], i1 = mesh.indices[t * 3 + 1], i2 = mesh.indices[t * 3 + 2];
				Rasterize(n, n, screen[i0], screen[i1], screen[i2], [&](int x, int y, float z, float w0, float w1, float w2) {
					const size_t index = static_cast<size_t>(y) * n + x;
					if (z >= depth[index]) return;
					const auto shaded = shade(mesh, i0, i1, i2, w0, w1, w2);
					const float alpha = shaded.a;
					auto& target = color[index];
					target = glm::vec4(glm::vec3(shaded) * alpha + glm::vec3(target) * (1.0f - alpha), alpha + target.a * (1.0f - alpha));
				});
			}
		}

		// Box filter down to the icon's size
		const float samples = static_cast<float>(supersample * supersample);
		for (int y = 0; y < size; y++) {
			for (int x = 0; x < size; x++) {
				glm::vec4 sum(0.0f);
				for (int sy = 0; sy < supersample; sy++) {
					for (int sx = 0; sx < supersample; sx++) sum += color[static_cast<size_t>(y * supersample + sy) * n + (x * supersample + sx)];
				}
				sum /= samples;
				uint8_t* out = &image.rgba[(static_cast<size_t>(y) * size + x) * 4];
				if (sum.a <= 0.0f) continue;
				out[0] = static_cast<uint8_t>(std::lround(ToSrgb(sum.r / sum.a) * 255.0f));
				out[1] = static_cast<uint8_t>(std::lround(ToSrgb(sum.g / sum.a) * 255.0f));
				out[2] = static_cast<uint8_t>(std::lround(ToSrgb(sum.b / sum.a) * 255.0f));
				out[3] = static_cast<uint8_t>(std::lround(std::clamp(sum.a, 0.0f, 1.0f) * 255.0f));
			}
		}
		return image;
	}
}
