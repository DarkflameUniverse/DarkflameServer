#include "UgcRender.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <unordered_map>

#include <glm/gtc/matrix_transform.hpp>

#include "UgcIconPose.h"
#include "UgcPalette.h"
#include "UgcThrottle.h"

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

	float ToLinear(float c) { return UgcPalette::SrgbToLinear(std::clamp(c, 0.0f, 1.0f)); }
	float ToSrgb(float c) { return UgcPalette::LinearToSrgb(std::clamp(c, 0.0f, 1.0f)); }

	// A bounding volume hierarchy over a mesh's triangles, for the occlusion rays
	class Bvh {
	public:
		explicit Bvh(const UgcModel::Mesh& mesh) : m_Mesh(mesh) {
			const size_t count = mesh.TriangleCount();
			m_Order.resize(count);
			std::iota(m_Order.begin(), m_Order.end(), 0u);
			m_Centers.resize(count);
			for (size_t t = 0; t < count; t++) m_Centers[t] = (Vertex(t, 0) + Vertex(t, 1) + Vertex(t, 2)) / 3.0f;
			if (count > 0) Build(0, static_cast<uint32_t>(count));
			Flatten();
		}

		// Whether a ray from `origin` along `direction` (unit) hits a triangle nearer than `maxDistance`
		bool Hits(const glm::vec3& origin, const glm::vec3& direction, float maxDistance) const {
			if (m_Nodes.empty()) return false;
			const glm::vec3 inverse(1.0f / (std::abs(direction.x) > 1e-12f ? direction.x : 1e-12f), 1.0f / (std::abs(direction.y) > 1e-12f ? direction.y : 1e-12f),
				1.0f / (std::abs(direction.z) > 1e-12f ? direction.z : 1e-12f));
			uint32_t stack[64];
			int top = 0;
			stack[top++] = 0;
			while (top > 0) {
				const auto& node = m_Nodes[stack[--top]];
				if (!BoxHit(node, origin, inverse, maxDistance)) continue;
				if (node.count > 0) {
					for (uint32_t i = node.first; i < node.first + node.count; i++) {
						if (TriangleHit(m_Triangles[i], origin, direction, maxDistance)) return true;
					}
				} else if (top < 62) {
					stack[top++] = node.first;
					stack[top++] = node.first + 1;
				}
			}
			return false;
		}

	private:
		struct Node {
			glm::vec3 min{};
			glm::vec3 max{};
			uint32_t first{}; // leaf: first triangle in m_Order; inner: the first of two children
			uint32_t count{}; // triangles, 0 for inner nodes
		};

		glm::vec3 Vertex(size_t t, int k) const { return m_Mesh.positions[m_Mesh.indices[t * 3 + k]]; }

		void Build(uint32_t first, uint32_t count) {
			// Iterative, so deep trees don't use the stack
			struct Task { uint32_t node, first, count; };
			m_Nodes.push_back({});
			std::vector<Task> tasks{ { 0, first, count } };
			while (!tasks.empty()) {
				const auto task = tasks.back();
				tasks.pop_back();
				Node node;
				node.min = glm::vec3(INF);
				node.max = glm::vec3(-INF);
				glm::vec3 centerMin(INF), centerMax(-INF);
				for (uint32_t i = task.first; i < task.first + task.count; i++) {
					for (int k = 0; k < 3; k++) {
						node.min = glm::min(node.min, Vertex(m_Order[i], k));
						node.max = glm::max(node.max, Vertex(m_Order[i], k));
					}
					centerMin = glm::min(centerMin, m_Centers[m_Order[i]]);
					centerMax = glm::max(centerMax, m_Centers[m_Order[i]]);
				}
				const auto extent = centerMax - centerMin;
				const int axis = extent.x >= extent.y && extent.x >= extent.z ? 0 : extent.y >= extent.z ? 1 : 2;
				if (task.count <= 4 || extent[axis] <= 0.0f) {
					node.first = task.first;
					node.count = task.count;
					m_Nodes[task.node] = node;
					continue;
				}
				const uint32_t half = task.count / 2;
				auto* begin = m_Order.data() + task.first;
				std::nth_element(begin, begin + half, begin + task.count, [&](uint32_t a, uint32_t b) { return m_Centers[a][axis] < m_Centers[b][axis]; });
				node.first = static_cast<uint32_t>(m_Nodes.size());
				node.count = 0;
				m_Nodes[task.node] = node;
				m_Nodes.push_back({});
				m_Nodes.push_back({});
				tasks.push_back({ node.first, task.first, half });
				tasks.push_back({ node.first + 1, task.first + half, task.count - half });
			}
		}

		static bool BoxHit(const Node& node, const glm::vec3& origin, const glm::vec3& inverse, float maxDistance) {
			const auto t0 = (node.min - origin) * inverse;
			const auto t1 = (node.max - origin) * inverse;
			const auto near = glm::min(t0, t1), far = glm::max(t0, t1);
			const float enter = std::max(std::max(near.x, near.y), std::max(near.z, 0.0f));
			const float exit = std::min(std::min(far.x, far.y), std::min(far.z, maxDistance));
			return enter <= exit;
		}

		struct Triangle {
			glm::vec3 a, e1, e2;
		};

		// The triangles in leaf order, edges worked out once (the rays read them far more often than the tree is built)
		void Flatten() {
			m_Triangles.reserve(m_Order.size());
			for (const auto t : m_Order) {
				const auto a = Vertex(t, 0);
				m_Triangles.push_back({ a, Vertex(t, 1) - a, Vertex(t, 2) - a });
			}
		}

		static bool TriangleHit(const Triangle& triangle, const glm::vec3& origin, const glm::vec3& direction, float maxDistance) {
			const auto& a = triangle.a;
			const auto& e1 = triangle.e1;
			const auto& e2 = triangle.e2;
			const auto p = glm::cross(direction, e2);
			const float det = glm::dot(e1, p);
			if (std::abs(det) < 1e-12f) return false;
			const float inv = 1.0f / det;
			const auto s = origin - a;
			const float u = glm::dot(s, p) * inv;
			if (u < 0.0f || u > 1.0f) return false;
			const auto q = glm::cross(s, e1);
			const float v = glm::dot(direction, q) * inv;
			if (v < 0.0f || u + v > 1.0f) return false;
			const float distance = glm::dot(e2, q) * inv;
			return distance > 1e-4f && distance < maxDistance;
		}

		const UgcModel::Mesh& m_Mesh;
		std::vector<Triangle> m_Triangles;
		std::vector<uint32_t> m_Order;
		std::vector<glm::vec3> m_Centers;
		std::vector<Node> m_Nodes;
	};

	float RadicalInverse(uint32_t bits) {
		bits = (bits << 16u) | (bits >> 16u);
		bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
		bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
		bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
		bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
		return static_cast<float>(bits) * 2.3283064365386963e-10f;
	}

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
		if (opaque.Empty() || !options.removeHidden) return result;

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

		for (const auto& direction : SphereDirections()) {
			// A ground plane under the model hides everything from below
			if (options.groundPlane && direction.y < -0.05f) continue;
			UgcThrottle::Checkpoint();
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
				if ((t & 0x3FFF) == 0) UgcThrottle::Checkpoint();
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

			// Triangles too small or thin to cover a pixel centre: kept when their centre isn't behind what was drawn.
			// Bigger ones that show would have covered one.
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

		for (size_t t = 0; t < triangles; t++) result.trianglesRemoved += visible[t] ? 0 : 1;
		result.kept = visible;
		UgcModel::KeepTriangles(opaque, visible);
		return result;
	}

	std::vector<float> AmbientOcclusion(const UgcModel::Mesh& mesh, const UgcModel::Mesh& occluders, float distance, int samples) {
		std::vector<float> ao(mesh.positions.size(), 1.0f);
		if (occluders.Empty() || samples <= 0 || distance <= 0.0f || mesh.normals.size() != mesh.positions.size()) return ao;
		const Bvh bvh(occluders);
		const auto count = static_cast<uint32_t>(samples);
		// Vertices at the same place facing the same way (bricks' shared corners) are worked out once
		struct Key {
			int32_t p[3], n[3];
			bool operator==(const Key& o) const { return std::equal(p, p + 3, o.p) && std::equal(n, n + 3, o.n); }
		};
		struct KeyHash {
			size_t operator()(const Key& k) const {
				size_t h = 1469598103934665603ull;
				for (int i = 0; i < 3; i++) h = (h ^ static_cast<uint32_t>(k.p[i])) * 1099511628211ull ^ static_cast<uint32_t>(k.n[i]) * 0x9E3779B97F4A7C15ull;
				return h;
			}
		};
		std::unordered_map<Key, float, KeyHash> known;
		known.reserve(mesh.positions.size());
		for (size_t v = 0; v < mesh.positions.size(); v++) {
			if ((v & 0xFF) == 0) UgcThrottle::Checkpoint();
			const auto& normal = mesh.normals[v];
			const Key key{ { static_cast<int32_t>(std::lround(mesh.positions[v].x * 1000.0f)), static_cast<int32_t>(std::lround(mesh.positions[v].y * 1000.0f)),
				static_cast<int32_t>(std::lround(mesh.positions[v].z * 1000.0f)) }, { static_cast<int32_t>(std::lround(normal.x * 100.0f)),
				static_cast<int32_t>(std::lround(normal.y * 100.0f)), static_cast<int32_t>(std::lround(normal.z * 100.0f)) } };
			if (const auto it = known.find(key); it != known.end()) {
				ao[v] = it->second;
				continue;
			}
			if (glm::dot(normal, normal) < 0.5f) continue;
			// A frame around the normal
			const glm::vec3 helper = std::abs(normal.x) < 0.9f ? glm::vec3(1, 0, 0) : glm::vec3(0, 1, 0);
			const auto tangent = glm::normalize(glm::cross(helper, normal));
			const auto bitangent = glm::cross(normal, tangent);
			// Hammersley points, turned by an amount of the vertex's own (fixed) so neighbours don't band
			const float turn = static_cast<float>((v * 0x9E3779B9u) >> 8 & 0xFFFFFF) / 16777216.0f;
			const auto origin = mesh.positions[v] + normal * 1e-3f;
			uint32_t open = 0;
			for (uint32_t i = 0; i < count; i++) {
				const float u = (i + 0.5f) / static_cast<float>(count);
				const float phi = 2.0f * 3.14159265f * std::fmod(RadicalInverse(i) + turn, 1.0f);
				const float r = std::sqrt(u), z = std::sqrt(std::max(0.0f, 1.0f - u));
				const auto direction = tangent * (r * std::cos(phi)) + bitangent * (r * std::sin(phi)) + normal * z;
				if (!bvh.Hits(origin, direction, distance)) open++;
			}
			ao[v] = static_cast<float>(open) / static_cast<float>(count);
			known.emplace(key, ao[v]);
		}
		return ao;
	}

	std::vector<float> BakeAo(UgcModel::Model& model, const AoOptions& options) {
		auto& opaque = model.opaque;
		if (!options.enabled || opaque.Empty()) return {};
		auto ao = AmbientOcclusion(opaque, opaque, options.distance, options.samples);
		const float strength = std::clamp(options.strength, 0.0f, 1.0f);
		for (size_t v = 0; v < opaque.colors.size() && v < ao.size(); v++) {
			glm::vec3 lit(1.0f - strength * (1.0f - ao[v]));
			if (v < opaque.glow.size()) lit += opaque.glow[v] * options.glowStrength;
			lit = glm::clamp(lit, 0.0f, 1.0f);
			auto& color = opaque.colors[v];
			color.r = ToSrgb(ToLinear(color.r) * lit.r);
			color.g = ToSrgb(ToLinear(color.g) * lit.g);
			color.b = ToSrgb(ToLinear(color.b) * lit.b);
		}
		return ao;
	}

	Image RenderIcon(const UgcModel::Model& source, const IconOptions& options, const std::vector<float>* opaqueAo) {
		const int size = std::clamp(options.size, 8, 1024);
		const int supersample = std::clamp(options.supersample, 1, 8);
		const int n = size * supersample;
		Image image{ size, size, std::vector<uint8_t>(static_cast<size_t>(size) * size * 4, 0) };
		if (source.Empty()) return image;

		UgcModel::Model model = source;
		const auto rotation = UgcIconPose::ModelRotation(options.modelYawDegrees, options.modelPitchDegrees, options.modelRollDegrees) * options.modelRotation;
		model.opaque.Transform(rotation);
		model.transparent.Transform(rotation);

		// The camera and the crop to the model's projected bounds (shared with the dashboard's pose editor)
		const auto frame = UgcIconPose::Compute({ &model.opaque.positions, &model.transparent.positions },
			{ options.yawDegrees, options.pitchDegrees, options.fovDegrees, options.margin, options.offsetX, options.offsetY });
		if (!frame.ok) return image;
		const glm::vec3 center = frame.center;
		const float radius = frame.radius;
		const glm::vec3 eye = frame.eye;
		const glm::vec3 dir = glm::normalize(eye - center);
		const auto project = [&](const glm::vec3& position) {
			const auto point = frame.IconPoint(position);
			return glm::vec3(point.x * n, point.y * n, point.z);
		};

		// Linear, premultiplied
		std::vector<glm::vec4> color(static_cast<size_t>(n) * n, glm::vec4(0.0f));
		std::vector<float> depth(static_cast<size_t>(n) * n, INF);
		const float sunYaw = glm::radians(options.sunYawDegrees), sunPitch = glm::radians(options.sunPitchDegrees);
		const glm::vec3 light = glm::normalize(glm::vec3(std::sin(sunYaw) * std::cos(sunPitch), std::sin(sunPitch), std::cos(sunYaw) * std::cos(sunPitch)));

		// Ambient occlusion darkens the world light (opaque bricks only, as they are what occludes)
		std::vector<float> ao;
		if (options.ao.enabled) {
			ao = opaqueAo && opaqueAo->size() == model.opaque.positions.size() ? *opaqueAo : AmbientOcclusion(model.opaque, model.opaque, options.ao.distance, options.ao.samples);
		}

		// The sun's shadows: a depth map seen from the sun, looked up with a few taps for the sun's soft edge
		const int shadowSize = 1024;
		const OrthoView sunView(center, radius * 1.05f, light, shadowSize);
		std::vector<float> shadowDepth;
		if (options.shadows > 0.0f) {
			shadowDepth.assign(static_cast<size_t>(shadowSize) * shadowSize, INF);
			const auto& mesh = model.opaque;
			std::vector<glm::vec3> screen(mesh.positions.size());
			for (size_t v = 0; v < mesh.positions.size(); v++) screen[v] = sunView.Project(mesh.positions[v]);
			for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
				if ((i & 0xFFFF) == 0) UgcThrottle::Checkpoint();
				Rasterize(shadowSize, shadowSize, screen[mesh.indices[i]], screen[mesh.indices[i + 1]], screen[mesh.indices[i + 2]], [&](int x, int y, float z, float, float, float) {
					auto& stored = shadowDepth[static_cast<size_t>(y) * shadowSize + x];
					stored = std::min(stored, z);
				});
			}
		}
		const float shadowBias = sunView.PixelSize() * 2.0f;
		const auto sunlit = [&](const glm::vec3& position) {
			if (shadowDepth.empty()) return 1.0f;
			const auto p = sunView.Project(position);
			float lit = 0.0f;
			for (int dy = -1; dy <= 1; dy++) {
				for (int dx = -1; dx <= 1; dx++) {
					const int x = static_cast<int>(p.x) + dx, y = static_cast<int>(p.y) + dy;
					if (x < 0 || y < 0 || x >= shadowSize || y >= shadowSize || p.z <= shadowDepth[static_cast<size_t>(y) * shadowSize + x] + shadowBias) lit += 1.0f;
				}
			}
			const float shadowed = 1.0f - lit / 9.0f;
			return 1.0f - std::clamp(options.shadows, 0.0f, 1.0f) * shadowed;
		};
		const glm::vec3 toCamera = glm::normalize(dir);
		const glm::vec3 halfway = glm::normalize(light + toCamera);

		const auto shade = [&](const UgcModel::Mesh& mesh, bool isOpaque, uint32_t i0, uint32_t i1, uint32_t i2, float w0, float w1, float w2) {
			glm::vec3 normal(0.0f, 1.0f, 0.0f);
			if (mesh.normals.size() == mesh.positions.size()) {
				normal = mesh.normals[i0] * w0 + mesh.normals[i1] * w1 + mesh.normals[i2] * w2;
				const auto length = glm::length(normal);
				normal = length > 0.0f ? normal / length : glm::vec3(0.0f, 1.0f, 0.0f);
				if (glm::dot(normal, dir) < 0.0f) normal = -normal; // the back of a face
			}
			glm::vec4 base(0.63f, 0.63f, 0.63f, 1.0f);
			if (mesh.colors.size() == mesh.positions.size()) base = mesh.colors[i0] * w0 + mesh.colors[i1] * w1 + mesh.colors[i2] * w2;
			const float occlusion = isOpaque && ao.size() == mesh.positions.size() ? ao[i0] * w0 + ao[i1] * w1 + ao[i2] * w2 : 1.0f;
			const float strength = std::clamp(options.ao.strength, 0.0f, 1.0f);
			const auto position = mesh.positions[i0] * w0 + mesh.positions[i1] * w1 + mesh.positions[i2] * w2;
			const float direct = std::max(0.0f, glm::dot(normal, light));
			const float sun = direct > 0.0f ? sunlit(position + normal * shadowBias) : 0.0f;
			// Diffuse: the world's light (radiance `ambient`), a fill from the camera and the sun's (irradiances, over pi)
			const float lighting = options.ambient * (1.0f - strength * (1.0f - occlusion)) +
				(options.fill * std::max(0.0f, glm::dot(normal, toCamera)) + options.sunStrength * direct * sun) / 3.14159265f;
			// The sun's highlight (Blinn-Phong), white, on top of the color
			const float highlight = direct > 0.0f ? options.specular * options.sunStrength / 3.14159265f * sun * std::pow(std::max(0.0f, glm::dot(normal, halfway)), std::max(options.shininess, 1.0f)) : 0.0f;
			const float exposure = std::max(options.exposure, 0.0f);
			return glm::vec4((ToLinear(base.r) * lighting + highlight) * exposure, (ToLinear(base.g) * lighting + highlight) * exposure,
				(ToLinear(base.b) * lighting + highlight) * exposure, std::clamp(base.a, 0.0f, 1.0f));
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
					const auto shaded = shade(mesh, true, i0, i1, i2, w0, w1, w2);
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
					const auto shaded = shade(mesh, false, i0, i1, i2, w0, w1, w2);
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
				// sRGB, then the contrast around its middle grey
				const auto tone = [&options](float linear) { return std::clamp(0.5f + (ToSrgb(linear) - 0.5f) * options.contrast, 0.0f, 1.0f); };
				out[0] = static_cast<uint8_t>(std::lround(tone(sum.r / sum.a) * 255.0f));
				out[1] = static_cast<uint8_t>(std::lround(tone(sum.g / sum.a) * 255.0f));
				out[2] = static_cast<uint8_t>(std::lround(tone(sum.b / sum.a) * 255.0f));
				out[3] = static_cast<uint8_t>(std::lround(std::clamp(sum.a, 0.0f, 1.0f) * 255.0f));
			}
		}
		return image;
	}
}
