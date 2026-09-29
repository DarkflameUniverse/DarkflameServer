#include "UgcRender.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <unordered_map>

#include <glm/gtc/matrix_transform.hpp>

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
}

namespace UgcRender {
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
			const auto look = mesh.looks.size() == mesh.positions.size() && (isOpaque || mesh.looks[i0] == UgcModel::eLook::GLITTER) ? mesh.looks[i0] : UgcModel::eLook::PLASTIC;
			if (look == UgcModel::eLook::GLITTER) {
				// LEGO-AnimUV: lerp(vertex color, the texture's white, its alpha), then lit as plastic
				// On the mesh's own UVs (read from the .nif: each brick's pattern placed as it was made), else projected
				const auto uv = mesh.uvs.size() == mesh.positions.size() ? mesh.uvs[i0] * w0 + mesh.uvs[i1] * w1 + mesh.uvs[i2] * w2 :
					UgcGlitter::Uv(position, normal, options.glitter.tile);
				const float fleck = UgcGlitter::Sample(glitterAlpha, uv);
				base = glm::vec4(glm::mix(glm::vec3(base), glm::vec3(1.0f), fleck), base.a);
			}
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
