#include "UgcRender.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <numeric>
#include <optional>
#include <type_traits>
#include <unordered_map>

#include <glm/gtc/matrix_transform.hpp>

#ifdef DLU_OIDN
#include <OpenImageDenoise/oidn.hpp>
#endif

#include "UgcIconPose.h"
#include "UgcPalette.h"
#include "UgcRays.h"
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

#ifdef DLU_OIDN
	// Open Image Denoise's CPU device for the thread, made the first time: one thread of its own (not all the cores),
	// whose time Denoise charges to the asking thread (UgcThrottle::Charge); null when it can't be made
	oidn::DeviceRef* OidnDevice() {
		thread_local std::optional<oidn::DeviceRef> device;
		thread_local bool tried = false;
		if (!tried) {
			tried = true;
			auto made = oidn::newDevice(oidn::DeviceType::CPU);
			if (made) {
				made.set("numThreads", 1);
				made.set("setAffinity", false);
				made.commit();
				const char* message = nullptr;
				if (made.getError(message) == oidn::Error::None) device = std::move(made);
			}
		}
		return device ? &*device : nullptr;
	}
#endif

	// Denoises `color` (linear, premultiplied, n x n) with Open Image Denoise's ray tracing filter, guided by `albedo`
	// and `normals` (noise free); false (and `color` as it was) when it can't
	bool Denoise(std::vector<glm::vec4>& color, const std::vector<glm::vec3>& albedo, const std::vector<glm::vec3>& normals, int n) {
#ifdef DLU_OIDN
		auto* device = OidnDevice();
		if (!device || n <= 0) return false;
		const size_t pixels = static_cast<size_t>(n) * n;
		std::vector<glm::vec3> input(pixels), output(pixels);
		for (size_t i = 0; i < pixels; i++) input[i] = glm::vec3(color[i]);
		auto filter = device->newFilter("RT");
		filter.setImage("color", input.data(), oidn::Format::Float3, n, n);
		filter.setImage("albedo", const_cast<glm::vec3*>(albedo.data()), oidn::Format::Float3, n, n);
		filter.setImage("normal", const_cast<glm::vec3*>(normals.data()), oidn::Format::Float3, n, n);
		filter.setImage("output", output.data(), oidn::Format::Float3, n, n);
		filter.set("hdr", true);
		filter.set("cleanAux", true);
		// It works on a thread of its own while this one waits: its time is this job's CPU time
		const auto started = std::chrono::steady_clock::now();
		filter.commit();
		filter.execute();
		UgcThrottle::Charge(std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count());
		const char* message = nullptr;
		if (device->getError(message) != oidn::Error::None) return false;
		// The shape's outline and coverage stay as drawn
		for (size_t i = 0; i < pixels; i++) {
			if (color[i].a > 0.0f) color[i] = glm::vec4(glm::max(output[i], glm::vec3(0.0f)), color[i].a);
		}
		return true;
#else
		(void)color;
		(void)albedo;
		(void)normals;
		(void)n;
		return false;
#endif
	}

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
	std::string_view Name(eDenoise denoise) {
		return denoise == eDenoise::OIDN ? "oidn" : "off";
	}

	std::optional<eDenoise> ParseDenoise(std::string_view name) {
		for (const auto denoise : { eDenoise::OFF, eDenoise::OIDN }) {
			if (Name(denoise) == name) return denoise;
		}
		return std::nullopt;
	}

	bool Available(eDenoise denoise) {
#ifdef DLU_OIDN
		if (denoise == eDenoise::OIDN) return OidnDevice() != nullptr;
#endif
		return denoise == eDenoise::OFF;
	}

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

	std::vector<bool> VisibleFromAround(const UgcModel::Model& model, int resolution, bool groundPlane) {
		const auto& opaque = model.opaque;
		const size_t triangles = opaque.TriangleCount();
		std::vector<bool> visible(triangles, false);
		if (opaque.Empty()) return visible;

		glm::vec3 center{};
		float radius{};
		Bounds(model, center, radius);
		radius *= 1.02f;
		resolution = std::clamp(resolution, 64, 4096);
		const size_t pixels = static_cast<size_t>(resolution) * resolution;
		std::vector<float> depth(pixels);
		std::vector<uint32_t> ids(pixels);
		std::vector<glm::vec3> screen(opaque.positions.size());

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
			if (groundPlane && direction.y < -0.05f) continue;
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
		return visible;
	}

	std::vector<float> AmbientOcclusion(const UgcModel::Mesh& mesh, const UgcModel::Mesh& occluders, float distance, int samples, UgcRays::eBackend rays) {
		std::vector<float> ao(mesh.positions.size(), 1.0f);
		if (occluders.Empty() || samples <= 0 || distance <= 0.0f || mesh.normals.size() != mesh.positions.size()) return ao;
		const auto scene = UgcRays::Make(rays, occluders);
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
				if (!scene->Occluded(origin, direction, 1e-4f, distance)) open++;
			}
			ao[v] = static_cast<float>(open) / static_cast<float>(count);
			known.emplace(key, ao[v]);
		}
		return ao;
	}

	std::vector<float> BakeAo(UgcModel::Model& model, const AoOptions& options) {
		auto& opaque = model.opaque;
		if (!options.enabled || opaque.Empty()) return {};
		auto ao = AmbientOcclusion(opaque, opaque, options.distance, options.samples, options.rays);
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

	Image RenderIcon(const UgcModel::Model& source, const IconOptions& options, const std::vector<float>* opaqueAo, const UgcModel::Model* plain) {
		const int size = std::clamp(options.size, 8, 1024);
		const int supersample = std::clamp(options.supersample, 1, 8);
		const int n = size * supersample;
		Image image{ size, size, std::vector<uint8_t>(static_cast<size_t>(size) * size * 4, 0) };
		if (source.Empty()) return image;

		// Denoised with the model before its bake: its occlusion is traced per pixel (noisy), then denoised
		const bool denoise = options.denoise != eDenoise::OFF && Available(options.denoise);
		const bool traced = denoise && plain && !plain->opaque.Empty() && options.denoiseSamples > 0 && options.bakedAo > 0.0f && options.ao.distance > 0.0f;
		UgcModel::Model model = traced ? *plain : source;
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
		if (options.ao.enabled && !traced) {
			ao = opaqueAo && opaqueAo->size() == model.opaque.positions.size() ? *opaqueAo : AmbientOcclusion(model.opaque, model.opaque, options.ao.distance, options.ao.samples, options.ao.rays);
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
		bool anyGlitter = false;
		for (const auto* mesh : { &model.opaque, &model.transparent }) {
			anyGlitter = anyGlitter || std::find(mesh->looks.begin(), mesh->looks.end(), UgcModel::eLook::GLITTER) != mesh->looks.end();
		}
		const auto glitterAlpha = anyGlitter ? UgcGlitter::FleckAlpha(options.glitter) : std::vector<uint8_t>{};

		// A point's surface: its normal (towards the camera), its color before the light (glitter's flecks on it), where
		// it is and how it looks
		struct Surface {
			glm::vec3 normal;
			glm::vec4 base;
			glm::vec3 position;
			UgcModel::eLook look;
		};
		const auto surface = [&](const UgcModel::Mesh& mesh, bool isOpaque, uint32_t i0, uint32_t i1, uint32_t i2, float w0, float w1, float w2) {
			glm::vec3 normal(0.0f, 1.0f, 0.0f);
			if (mesh.normals.size() == mesh.positions.size()) {
				normal = mesh.normals[i0] * w0 + mesh.normals[i1] * w1 + mesh.normals[i2] * w2;
				const auto length = glm::length(normal);
				normal = length > 0.0f ? normal / length : glm::vec3(0.0f, 1.0f, 0.0f);
				if (glm::dot(normal, dir) < 0.0f) normal = -normal; // the back of a face
			}
			glm::vec4 base(0.63f, 0.63f, 0.63f, 1.0f);
			if (mesh.colors.size() == mesh.positions.size()) base = mesh.colors[i0] * w0 + mesh.colors[i1] * w1 + mesh.colors[i2] * w2;
			const auto position = mesh.positions[i0] * w0 + mesh.positions[i1] * w1 + mesh.positions[i2] * w2;
			const auto look = mesh.looks.size() == mesh.positions.size() && (isOpaque || mesh.looks[i0] == UgcModel::eLook::GLITTER) ? mesh.looks[i0] : UgcModel::eLook::PLASTIC;
			if (look == UgcModel::eLook::GLITTER) {
				// LEGO-AnimUV: lerp(vertex color, the texture's white, its alpha), then lit as plastic
				// On the mesh's own UVs (read from the .nif: each brick's pattern placed as it was made), else projected
				const auto uv = mesh.uvs.size() == mesh.positions.size() ? mesh.uvs[i0] * w0 + mesh.uvs[i1] * w1 + mesh.uvs[i2] * w2 :
					UgcGlitter::Uv(position, normal, options.glitter.tile);
				const float fleck = UgcGlitter::Sample(glitterAlpha, uv);
				base = glm::vec4(glm::mix(glm::vec3(base), glm::vec3(1.0f), fleck), base.a);
			}
			return Surface{ normal, base, position, look };
		};

		const auto shade = [&](const UgcModel::Mesh& mesh, bool isOpaque, uint32_t i0, uint32_t i1, uint32_t i2, float w0, float w1, float w2) {
			const auto point = surface(mesh, isOpaque, i0, i1, i2, w0, w1, w2);
			const auto& normal = point.normal;
			const auto& base = point.base;
			const auto& position = point.position;
			const auto look = point.look;
			const float occlusion = isOpaque && ao.size() == mesh.positions.size() ? ao[i0] * w0 + ao[i1] * w1 + ao[i2] * w2 : 1.0f;
			const float strength = std::clamp(options.ao.strength, 0.0f, 1.0f);
			const float direct = std::max(0.0f, glm::dot(normal, light));
			const float sun = direct > 0.0f ? sunlit(position + normal * shadowBias) : 0.0f;
			// Diffuse: the world's light (radiance `ambient`), a fill from the camera and the sun's (irradiances, over pi)
			const float lighting = options.ambient * (1.0f - strength * (1.0f - occlusion)) +
				(options.fill * std::max(0.0f, glm::dot(normal, toCamera)) + options.sunStrength * direct * sun) / 3.14159265f;
			// The sun's highlight (Blinn-Phong), white, on top of the color
			const float highlight = direct > 0.0f ? options.specular * options.sunStrength / 3.14159265f * sun * std::pow(std::max(0.0f, glm::dot(normal, halfway)), std::max(options.shininess, 1.0f)) : 0.0f;
			const float exposure = std::max(options.exposure, 0.0f);
			if (look == UgcModel::eLook::PLASTIC || look == UgcModel::eLook::GLITTER) {
				return glm::vec4((ToLinear(base.r) * lighting + highlight) * exposure, (ToLinear(base.g) * lighting + highlight) * exposure,
					(ToLinear(base.b) * lighting + highlight) * exposure, std::clamp(base.a, 0.0f, 1.0f));
			}
			const glm::vec3 linear(ToLinear(base.r), ToLinear(base.g), ToLinear(base.b));
			glm::vec3 shaded = (linear * lighting + glm::vec3(highlight)) * exposure;
			if (look == UgcModel::eLook::METAL || look == UgcModel::eLook::BRUSHED) {
				// A reflection of a bright sky over a dark ground, tinted by the color (polished: sharp; brushed: blurred
				// and duller), over a dimmed diffuse light, and the sun's highlight in the metal's color
				const bool polished = look == UgcModel::eLook::METAL;
				const auto reflected = glm::reflect(-toCamera, normal);
				const float up = polished ? glm::smoothstep(-0.15f, 0.5f, reflected.y) : 0.5f + 0.5f * reflected.y;
				const float environment = glm::mix(0.06f, polished ? 1.1f : 0.75f, up);
				const float spot = direct > 0.0f ? options.sunStrength / 3.14159265f * sun *
					std::pow(std::max(0.0f, glm::dot(normal, halfway)), polished ? 180.0f : 30.0f) * (polished ? 4.0f : 1.2f) : 0.0f;
				shaded = linear * (lighting * 0.35f + environment + spot) * exposure;
			} else if (look == UgcModel::eLook::GLOW) {
				// LEGO-Emissive: lerp(lit, vertex color, vertex alpha * the material's emissive red)
				shaded = glm::mix(shaded, linear, std::clamp(options.glowEmissive, 0.0f, 1.0f));
			}
			return glm::vec4(shaded, std::clamp(base.a, 0.0f, 1.0f));
		};

		// Opaque first, with the depth buffer
		// Traced: each pixel's point, for the occlusion traced after
		std::vector<glm::vec3> points, pointNormals;
		if (traced) {
			points.assign(color.size(), glm::vec3(0.0f));
			pointNormals.assign(color.size(), glm::vec3(0.0f));
		}
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
					if (traced) {
						const auto point = surface(mesh, true, i0, i1, i2, w0, w1, w2);
						points[index] = point.position;
						pointNormals[index] = point.normal;
					}
				});
			}
		}
		// Traced: every opaque pixel's occlusion from a few rays (cosine weighted around its normal, a pattern turned
		// per pixel, so the error differs from pixel to pixel as the denoiser expects), darkening it as the bake darkens
		// the vertex colors (LU Toolbox's Bake Lighting: the color times 1 - strength x (1 - occlusion))
		if (traced) {
			const auto scene = UgcRays::Make(options.ao.rays, model.opaque);
			const auto count = static_cast<uint32_t>(std::clamp(options.denoiseSamples, 1, 256));
			for (size_t index = 0; index < color.size(); index++) {
				if ((index & 0x3FF) == 0) UgcThrottle::Checkpoint();
				if (color[index].a <= 0.0f) continue;
				const auto& normal = pointNormals[index];
				const glm::vec3 helper = std::abs(normal.x) < 0.9f ? glm::vec3(1, 0, 0) : glm::vec3(0, 1, 0);
				const auto tangent = glm::normalize(glm::cross(helper, normal));
				const auto bitangent = glm::cross(normal, tangent);
				uint32_t hash = static_cast<uint32_t>(index) * 0x9E3779B9u;
				hash ^= hash >> 16;
				hash *= 0x85EBCA6Bu;
				hash ^= hash >> 13;
				const float turn = static_cast<float>(hash >> 8) / 16777216.0f;
				const float shift = static_cast<float>((hash * 0xC2B2AE35u) >> 8) / 16777216.0f;
				const auto origin = points[index] + normal * 1e-3f;
				uint32_t open = 0;
				for (uint32_t i = 0; i < count; i++) {
					const float u = std::fmod((i + shift) / static_cast<float>(count), 1.0f);
					const float phi = 2.0f * 3.14159265f * std::fmod(RadicalInverse(i) + turn, 1.0f);
					const float r = std::sqrt(u), up = std::sqrt(std::max(0.0f, 1.0f - u));
					const auto direction = tangent * (r * std::cos(phi)) + bitangent * (r * std::sin(phi)) + normal * up;
					if (!scene->Occluded(origin, direction, 1e-4f, options.ao.distance)) open++;
				}
				const float occlusion = static_cast<float>(open) / static_cast<float>(count);
				const float lit = 1.0f - std::clamp(options.bakedAo, 0.0f, 1.0f) * (1.0f - occlusion);
				color[index] = glm::vec4(glm::vec3(color[index]) * lit, color[index].a);
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

		// Box filter down to the icon's size (linear, premultiplied)
		const float samples = static_cast<float>(supersample * supersample);
		const auto boxFilter = [&](const auto& full) {
			using Pixel = typename std::decay_t<decltype(full)>::value_type;
			std::vector<Pixel> small(static_cast<size_t>(size) * size, Pixel(0.0f));
			for (int y = 0; y < size; y++) {
				for (int x = 0; x < size; x++) {
					Pixel sum(0.0f);
					for (int sy = 0; sy < supersample; sy++) {
						for (int sx = 0; sx < supersample; sx++) sum += full[static_cast<size_t>(y * supersample + sy) * n + (x * supersample + sx)];
					}
					sum /= samples;
					small[static_cast<size_t>(y) * size + x] = sum;
				}
			}
			return small;
		};
		auto filtered = boxFilter(color);

		// Denoised at the icon's size (the supersampling has averaged the pixels already; a denoiser's time grows with
		// the pixels), guided by the colors before the light and the normals of the same view, which have no noise
		if (denoise) {
			std::vector<glm::vec3> albedo(static_cast<size_t>(n) * n, glm::vec3(0.0f)), normals(static_cast<size_t>(n) * n, glm::vec3(0.0f));
			// The colors before the bake when known (traced: the model drawn)
			UgcModel::Model guide = plain && !traced ? *plain : UgcModel::Model{};
			if (plain && !traced) {
				guide.opaque.Transform(rotation);
				guide.transparent.Transform(rotation);
			}
			const auto& drawn = plain && !traced ? guide : model;
			std::vector<float> guideDepth(static_cast<size_t>(n) * n, INF);
			for (const bool isOpaque : { true, false }) {
				const auto& mesh = isOpaque ? drawn.opaque : drawn.transparent;
				std::vector<glm::vec3> screen(mesh.positions.size());
				for (size_t v = 0; v < mesh.positions.size(); v++) screen[v] = project(mesh.positions[v]);
				for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
					const auto i0 = mesh.indices[i], i1 = mesh.indices[i + 1], i2 = mesh.indices[i + 2];
					Rasterize(n, n, screen[i0], screen[i1], screen[i2], [&](int x, int y, float z, float w0, float w1, float w2) {
						const size_t index = static_cast<size_t>(y) * n + x;
						// Transparent surfaces over the opaque ones, in any order (a guide needn't be exact)
						if (z >= guideDepth[index]) return;
						const auto point = surface(mesh, isOpaque, i0, i1, i2, w0, w1, w2);
						const glm::vec3 linear(ToLinear(point.base.r), ToLinear(point.base.g), ToLinear(point.base.b));
						if (isOpaque) {
							guideDepth[index] = z;
							albedo[index] = linear;
							normals[index] = point.normal;
						} else {
							const float alpha = std::clamp(point.base.a, 0.0f, 1.0f);
							albedo[index] = glm::mix(albedo[index], linear, alpha);
							normals[index] = glm::normalize(glm::mix(normals[index], point.normal, alpha) + glm::vec3(1e-6f));
						}
					});
				}
			}
			Denoise(filtered, boxFilter(albedo), boxFilter(normals), size);
		}

		for (int y = 0; y < size; y++) {
			for (int x = 0; x < size; x++) {
				const auto& sum = filtered[static_cast<size_t>(y) * size + x];
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
