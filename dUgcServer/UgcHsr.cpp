#include "UgcHsr.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numeric>

#include "UgcThrottle.h"

namespace {
	constexpr float INF = std::numeric_limits<float>::infinity();
	constexpr float PI = 3.14159265358979f;
	// Blender's default Glossy bounces (LU Toolbox leaves it)
	constexpr int GLOSSY_BOUNCES = 4;
	// Points on one triangle at most (very big triangles get their points further apart)
	constexpr size_t MAX_POINTS = 4096;

	// PCG32 (O'Neill), seeded per path so every path is the same whatever order they're traced in
	class Random {
	public:
		explicit Random(uint64_t seed) {
			m_State = 0;
			Next();
			m_State += seed;
			Next();
		}

		uint32_t Next() {
			const uint64_t old = m_State;
			m_State = old * 6364136223846793005ull + 1442695040888963407ull;
			const auto xorshifted = static_cast<uint32_t>(((old >> 18u) ^ old) >> 27u);
			const auto rot = static_cast<uint32_t>(old >> 59u);
			return (xorshifted >> rot) | (xorshifted << ((32u - rot) & 31u));
		}

		// In [0, 1)
		float Float() { return static_cast<float>(Next() >> 8) * (1.0f / 16777216.0f); }

	private:
		uint64_t m_State;
	};

	uint64_t Mix(uint64_t h) {
		h ^= h >> 33;
		h *= 0xff51afd7ed558ccdull;
		h ^= h >> 33;
		h *= 0xc4ceb9fe1a85ec53ull;
		h ^= h >> 33;
		return h;
	}

	uint64_t PathSeed(uint64_t seed, uint64_t triangle, uint64_t point, uint64_t sample) {
		return Mix(Mix(Mix(seed ^ 0x9E3779B97F4A7C15ull) ^ triangle) ^ (point << 16 | sample));
	}

	// Directions around a unit normal (Duff et al., "Building an Orthonormal Basis, Revisited")
	void Basis(const glm::vec3& n, glm::vec3& t, glm::vec3& b) {
		const float sign = std::copysign(1.0f, n.z);
		const float a = -1.0f / (sign + n.z);
		const float c = n.x * n.y * a;
		t = glm::vec3(1.0f + sign * n.x * n.x * a, sign * c, -sign * n.x);
		b = glm::vec3(c, sign + n.y * n.y * a, -n.y);
	}

	glm::vec3 CosineHemisphere(const glm::vec3& n, float u, float v) {
		glm::vec3 t, b;
		Basis(n, t, b);
		const float r = std::sqrt(u), phi = 2.0f * PI * v;
		return t * (r * std::cos(phi)) + b * (r * std::sin(phi)) + n * std::sqrt(std::max(0.0f, 1.0f - u));
	}

	/**
	 * Cycles' ensure_valid_reflection, which the Principled BSDF applies to its normal: N turned towards Ng just
	 * enough that the mirror reflection of I (towards where the light goes) stays above the surface.
	 */
	glm::vec3 EnsureValidReflection(const glm::vec3& ng, const glm::vec3& i, const glm::vec3& n) {
		const auto r = 2.0f * glm::dot(n, i) * n - i;
		const float threshold = std::min(0.9f * glm::dot(ng, i), 0.01f);
		if (glm::dot(ng, r) >= threshold) return n;
		const float ndotng = glm::dot(n, ng);
		const auto x = glm::normalize(n - ndotng * ng);
		const float ix = glm::dot(i, x), iz = glm::dot(i, ng);
		const float ix2 = ix * ix, iz2 = iz * iz;
		const float a = ix2 + iz2;
		const float b = std::sqrt(std::max(ix2 * (a - threshold * threshold), 0.0f));
		const float c = iz * threshold + a;
		const float fac = 0.5f / a;
		const float n1z2 = fac * (b + c), n2z2 = fac * (-b + c);
		bool valid1 = n1z2 > 1e-5f && n1z2 <= 1.0f + 1e-5f;
		bool valid2 = n2z2 > 1e-5f && n2z2 <= 1.0f + 1e-5f;
		const auto root = [](float v) { return std::sqrt(std::max(v, 0.0f)); };
		glm::vec2 chosen;
		if (valid1 && valid2) {
			const glm::vec2 n1(root(1.0f - n1z2), root(n1z2)), n2(root(1.0f - n2z2), root(n2z2));
			const float r1 = 2.0f * (n1.x * ix + n1.y * iz) * n1.y - iz;
			const float r2 = 2.0f * (n2.x * ix + n2.y * iz) * n2.y - iz;
			valid1 = r1 >= 1e-5f;
			valid2 = r2 >= 1e-5f;
			if (valid1 && valid2) chosen = r1 < r2 ? n1 : n2;
			else chosen = r1 > r2 ? n1 : n2;
		} else if (valid1 || valid2) {
			const float z2 = valid1 ? n1z2 : n2z2;
			chosen = glm::vec2(root(1.0f - z2), root(z2));
		} else {
			return ng;
		}
		return chosen.x * x + chosen.y * ng;
	}

	float SchlickFresnel(float u) {
		const float m = std::clamp(1.0f - u, 0.0f, 1.0f);
		const float m2 = m * m;
		return m2 * m2 * m;
	}

	float FresnelDielectricCos(float cosi, float eta) {
		const float c = std::abs(cosi);
		float g = eta * eta - 1.0f + c * c;
		if (!(g > 0.0f)) return 1.0f;
		g = std::sqrt(g);
		const float a = (g - c) / (g + c);
		const float b = (c * (g + c) - 1.0f) / (c * (g - c) + 1.0f);
		return 0.5f * a * a * (1.0f + b * b);
	}

	/**
	 * The bake material: a new material's Principled BSDF (Blender 3.1: base color 0.8, specular 0.5, roughness 0.5,
	 * GGX) as Cycles 3.1 samples it, a diffuse closure (Burley, with retro-reflection) and a specular one (GGX with a
	 * dielectric Fresnel, IOR 1.5) picked by their sample weights. Only the directions a path takes and its throughput
	 * (for the Russian roulette) matter here.
	 */
	class Principled {
	public:
		static constexpr float BASE = 0.8f;      // Base Color
		static constexpr float ROUGHNESS = 0.5f; // Roughness
		static constexpr float IOR = 1.5f;       // (2 / (1 - sqrt(0.08 * Specular 0.5))) - 1
		static constexpr float CSPEC0 = 0.04f;   // Specular 0.5 * 0.08

		// n: the material's normal (after EnsureValidReflection), i: towards where the path came from, ng: the
		// geometric normal on the same side, sn: the shading normal. `first`: the bake point (Cycles' defensive
		// sampling there); minRayPdf: the smallest bounce pdf so far (Cycles' Filter Glossy 1 blurs the specular)
		Principled(const glm::vec3& n, const glm::vec3& i, const glm::vec3& ng, const glm::vec3& sn, bool first, float minRayPdf)
			: m_N(n), m_I(i), m_Ng(ng), m_Sn(sn) {
			m_F0 = FresnelDielectricCos(1.0f, IOR);
			m_WeightDiffuse = BASE;
			m_WeightSpecular = Fresnel(m_I, m_N); // the closure's weight 1 times its Fresnel color's average
			if (first) {
				const float sum = m_WeightDiffuse + m_WeightSpecular;
				m_WeightDiffuse = std::max(m_WeightDiffuse, 0.125f * sum);
				m_WeightSpecular = std::max(m_WeightSpecular, 0.125f * sum);
			}
			m_Alpha = std::clamp(ROUGHNESS * ROUGHNESS, 0.0f, 1.0f);
			if (minRayPdf < 1.0f) m_Alpha = std::max(std::sqrt(1.0f - minRayPdf) * 0.5f, m_Alpha);
		}

		struct Sample {
			glm::vec3 direction{};
			float pdf{};        // of the mixture; 0: nothing sampled, the path ends
			float throughput{}; // eval / pdf
			bool glossy{};
		};

		Sample Draw(Random& random) const {
			Sample sample;
			const float pick = random.Float() * (m_WeightDiffuse + m_WeightSpecular);
			const float u = random.Float(), v = random.Float();
			float pdfDiffuse = 0.0f, pdfSpecular = 0.0f;
			float eval = 0.0f;
			if (pick < m_WeightDiffuse) {
				sample.direction = CosineHemisphere(m_N, u, v);
				if (!(glm::dot(m_Ng, sample.direction) > 0.0f)) return sample;
				eval = EvalDiffuse(sample.direction, pdfDiffuse);
				if (!(pdfDiffuse > 0.0f) || !(eval > 0.0f)) return sample;
				if (!Transmission(sample.direction)) eval += EvalSpecular(sample.direction, pdfSpecular);
			} else {
				sample.glossy = true;
				if (!SampleSpecular(u, v, sample.direction, eval, pdfSpecular)) return sample;
				if (!Transmission(sample.direction)) eval += EvalDiffuse(sample.direction, pdfDiffuse);
			}
			sample.pdf = (pdfDiffuse * m_WeightDiffuse + pdfSpecular * m_WeightSpecular) / (m_WeightDiffuse + m_WeightSpecular);
			sample.throughput = sample.pdf > 0.0f ? eval / sample.pdf : 0.0f;
			return sample;
		}

	private:
		// Cycles decides reflection or transmission by the shading normal; both closures only reflect
		bool Transmission(const glm::vec3& l) const { return glm::dot(m_Sn, l) < 0.0f; }

		float Fresnel(const glm::vec3& l, const glm::vec3& h) const {
			const float fh = (FresnelDielectricCos(glm::dot(l, h), IOR) - m_F0) / (1.0f - m_F0);
			return CSPEC0 * (1.0f - fh) + fh;
		}

		// bsdf_principled_diffuse_eval_reflect times the closure weight
		float EvalDiffuse(const glm::vec3& l, float& pdf) const {
			const float nl = glm::dot(m_N, l);
			if (!(nl > 0.0f)) {
				pdf = 0.0f;
				return 0.0f;
			}
			pdf = nl / PI;
			const float nv = glm::dot(m_N, m_I);
			const float fv = SchlickFresnel(nv), fl = SchlickFresnel(nl);
			float f = (1.0f - 0.5f * fv) * (1.0f - 0.5f * fl);
			const float rr = ROUGHNESS * (glm::dot(l, m_I) + 1.0f);
			f += rr * (fl + fv + fl * fv * (rr - 1.0f));
			return BASE * nl / PI * f;
		}

		// bsdf_microfacet_ggx_eval_reflect (isotropic, Fresnel) times the closure weight 1
		float EvalSpecular(const glm::vec3& l, float& pdf) const {
			pdf = 0.0f;
			const float cosNO = glm::dot(m_N, m_I), cosNI = glm::dot(m_N, l);
			if (!(cosNI > 0.0f && cosNO > 0.0f) || m_Alpha * m_Alpha <= 1e-7f) return 0.0f;
			const auto m = glm::normalize(l + m_I);
			const float alpha2 = m_Alpha * m_Alpha;
			const float cosThetaM = glm::dot(m_N, m);
			const float cosThetaM2 = cosThetaM * cosThetaM;
			const float tanThetaM2 = (1.0f - cosThetaM2) / cosThetaM2;
			const float d = alpha2 / (PI * cosThetaM2 * cosThetaM2 * (alpha2 + tanThetaM2) * (alpha2 + tanThetaM2));
			const float g1o = 2.0f / (1.0f + std::sqrt(std::max(0.0f, 1.0f + alpha2 * (1.0f - cosNO * cosNO) / (cosNO * cosNO))));
			const float g1i = 2.0f / (1.0f + std::sqrt(std::max(0.0f, 1.0f + alpha2 * (1.0f - cosNI * cosNI) / (cosNI * cosNI))));
			const float common = d * 0.25f / cosNO;
			pdf = g1o * common;
			return Fresnel(l, m) * g1o * g1i * common;
		}

		// bsdf_microfacet_ggx_sample (isotropic): a visible normal (Heitz and d'Eon), the view reflected in it
		bool SampleSpecular(float randu, float randv, glm::vec3& l, float& eval, float& pdf) const {
			const float cosNO = glm::dot(m_N, m_I);
			if (!(cosNO > 0.0f)) return false;
			glm::vec3 x, y;
			Basis(m_N, x, y);
			// Stretch the view, sample the slopes, rotate and unstretch
			auto local = glm::normalize(glm::vec3(m_Alpha * glm::dot(x, m_I), m_Alpha * glm::dot(y, m_I), cosNO));
			float costheta = 1.0f, sintheta = 0.0f, cosphi = 1.0f, sinphi = 0.0f;
			if (local.z < 0.99999f) {
				costheta = local.z;
				sintheta = std::sqrt(std::max(0.0f, 1.0f - costheta * costheta));
				cosphi = local.x / sintheta;
				sinphi = local.y / sintheta;
			}
			float slopeX, slopeY, g1o;
			if (costheta >= 0.99999f) {
				const float r = std::sqrt(randu / (1.0f - randu));
				const float phi = 2.0f * PI * randv;
				slopeX = r * std::cos(phi);
				slopeY = r * std::sin(phi);
				g1o = 1.0f;
			} else {
				const float tanThetaI = sintheta / costheta;
				const float g1Inv = 0.5f * (1.0f + std::sqrt(std::max(0.0f, 1.0f + tanThetaI * tanThetaI)));
				g1o = 1.0f / g1Inv;
				const float a = 2.0f * randu * g1Inv - 1.0f;
				const float aa = a * a;
				const float tmp = 1.0f / (aa - 1.0f);
				const float b = tanThetaI, bb = b * b;
				const float dd = std::sqrt(std::max(0.0f, bb * (tmp * tmp) - (aa - bb) * tmp));
				const float x1 = b * tmp - dd, x2 = b * tmp + dd;
				slopeX = (a < 0.0f || x2 * tanThetaI > 1.0f) ? x1 : x2;
				float s;
				if (randv > 0.5f) {
					s = 1.0f;
					randv = 2.0f * (randv - 0.5f);
				} else {
					s = -1.0f;
					randv = 2.0f * (0.5f - randv);
				}
				const float z = (randv * (randv * (randv * 0.27385f - 0.73369f) + 0.46341f)) / (randv * (randv * (randv * 0.093073f + 0.309420f) - 1.0f) + 0.597999f);
				slopeY = s * z * std::sqrt(std::max(0.0f, 1.0f + slopeX * slopeX));
			}
			const float rotated = cosphi * slopeX - sinphi * slopeY;
			slopeY = sinphi * slopeX + cosphi * slopeY;
			slopeX = rotated * m_Alpha;
			slopeY *= m_Alpha;
			const auto localM = glm::normalize(glm::vec3(-slopeX, -slopeY, 1.0f));
			const auto m = x * localM.x + y * localM.y + m_N * localM.z;
			const float cosMO = glm::dot(m, m_I);
			if (!(cosMO > 0.0f)) return false;
			l = 2.0f * cosMO * m - m_I;
			if (!(glm::dot(m_Ng, l) > 0.0f)) return false;
			const float alpha2 = m_Alpha * m_Alpha;
			const float cosThetaM2 = localM.z * localM.z;
			const float tanThetaM2 = 1.0f / cosThetaM2 - 1.0f;
			const float d = alpha2 / (PI * cosThetaM2 * cosThetaM2 * (alpha2 + tanThetaM2) * (alpha2 + tanThetaM2));
			const float cosNI = glm::dot(m_N, l);
			const float g1i = 2.0f / (1.0f + std::sqrt(std::max(0.0f, 1.0f + alpha2 * (1.0f - cosNI * cosNI) / (cosNI * cosNI))));
			const float common = g1o * d * 0.25f / cosNO;
			pdf = common;
			eval = g1i * common * Fresnel(l, m);
			return pdf > 0.0f && eval > 0.0f;
		}

		glm::vec3 m_N, m_I, m_Ng, m_Sn;
		float m_F0{};
		float m_WeightDiffuse{}, m_WeightSpecular{};
		float m_Alpha{};
	};

	struct Hit {
		float t{ INF };
		uint32_t triangle{ UINT32_MAX };
		float u{}, v{}; // weights of the triangle's second and third vertex
	};

	/**
	 * A bounding volume hierarchy (binned surface area heuristic) over a mesh's triangles, for the paths' rays: the
	 * nearest hit. A ray never hits the triangle it leaves (`skip`), as in Cycles.
	 */
	class Bvh {
	public:
		explicit Bvh(const UgcModel::Mesh& mesh) {
			const size_t count = mesh.TriangleCount();
			std::vector<uint32_t> order(count);
			std::iota(order.begin(), order.end(), 0u);
			std::vector<glm::vec3> lo(count), hi(count), centre(count);
			for (size_t t = 0; t < count; t++) {
				const auto& a = mesh.positions[mesh.indices[t * 3]];
				const auto& b = mesh.positions[mesh.indices[t * 3 + 1]];
				const auto& c = mesh.positions[mesh.indices[t * 3 + 2]];
				lo[t] = glm::min(a, glm::min(b, c));
				hi[t] = glm::max(a, glm::max(b, c));
				centre[t] = (lo[t] + hi[t]) * 0.5f;
			}
			if (count > 0) Build(order, lo, hi, centre);
			m_Triangles.reserve(count);
			for (const auto t : order) {
				const auto& a = mesh.positions[mesh.indices[t * 3]];
				m_Triangles.push_back({ a, mesh.positions[mesh.indices[t * 3 + 1]] - a, mesh.positions[mesh.indices[t * 3 + 2]] - a, t });
			}
		}

		// The nearest triangle along the ray (unit direction) before `maxT`
		Hit Closest(const glm::vec3& origin, const glm::vec3& direction, uint32_t skip, float maxT = INF) const {
			Hit hit;
			hit.t = maxT;
			if (m_Nodes.empty()) return hit;
			const auto inverse = Inverse(direction);
			uint32_t stack[128];
			int top = 0;
			uint32_t index = 0;
			while (true) {
				const auto& node = m_Nodes[index];
				if (node.count > 0) {
					for (uint32_t i = node.first; i < node.first + node.count; i++) Intersect(m_Triangles[i], origin, direction, skip, hit);
				} else {
					const uint32_t left = node.first, right = node.first + 1;
					const float tl = Enter(m_Nodes[left], origin, inverse, hit.t);
					const float tr = Enter(m_Nodes[right], origin, inverse, hit.t);
					if (tl <= tr) {
						if (tr != INF && top < 128) stack[top++] = right;
						if (tl != INF) { index = left; continue; }
					} else {
						if (tl != INF && top < 128) stack[top++] = left;
						index = right;
						continue;
					}
				}
				// Next from the stack, skipping nodes now farther than the nearest hit
				bool found = false;
				while (top > 0) {
					index = stack[--top];
					if (Enter(m_Nodes[index], origin, inverse, hit.t) != INF) {
						found = true;
						break;
					}
				}
				if (!found) break;
			}
			return hit;
		}

	private:
		struct Node {
			glm::vec3 min{ INF };
			uint32_t first{}; // leaf: first triangle; inner: the left child (the right one follows it)
			glm::vec3 max{ -INF };
			uint32_t count{}; // triangles, 0 for inner nodes
		};

		// Where the ray enters the node's box, INF when it misses it before `maxT`
		static float Enter(const Node& node, const glm::vec3& origin, const glm::vec3& inverse, float maxT) {
			const auto t0 = (node.min - origin) * inverse;
			const auto t1 = (node.max - origin) * inverse;
			const auto near = glm::min(t0, t1), far = glm::max(t0, t1);
			const float enter = std::max(std::max(near.x, near.y), std::max(near.z, 0.0f));
			const float exit = std::min(std::min(far.x, far.y), std::min(far.z, maxT));
			return enter <= exit ? enter : INF;
		}

		struct Triangle {
			glm::vec3 a, e1, e2;
			uint32_t index;
		};

		static glm::vec3 Inverse(const glm::vec3& d) {
			const auto safe = [](float x) { return 1.0f / (std::abs(x) > 1e-20f ? x : std::copysign(1e-20f, x)); };
			return { safe(d.x), safe(d.y), safe(d.z) };
		}

		// Möller-Trumbore; keeps the hit when it's nearer than hit.t
		static bool Intersect(const Triangle& tri, const glm::vec3& origin, const glm::vec3& direction, uint32_t skip, Hit& hit) {
			if (tri.index == skip) return false;
			const auto p = glm::cross(direction, tri.e2);
			const float det = glm::dot(tri.e1, p);
			if (det == 0.0f) return false;
			const float inv = 1.0f / det;
			const auto s = origin - tri.a;
			const float u = glm::dot(s, p) * inv;
			if (u < 0.0f || u > 1.0f) return false;
			const auto q = glm::cross(s, tri.e1);
			const float v = glm::dot(direction, q) * inv;
			if (v < 0.0f || u + v > 1.0f) return false;
			const float t = glm::dot(tri.e2, q) * inv;
			if (!(t > 0.0f) || t >= hit.t) return false;
			hit.t = t;
			hit.triangle = tri.index;
			hit.u = u;
			hit.v = v;
			return true;
		}

		void Build(std::vector<uint32_t>& order, const std::vector<glm::vec3>& lo, const std::vector<glm::vec3>& hi, const std::vector<glm::vec3>& centre) {
			constexpr int BINS = 16;
			constexpr uint32_t LEAF = 4;
			struct Task { uint32_t node, first, count; };
			const auto area = [](const glm::vec3& min, const glm::vec3& max) {
				const auto d = glm::max(max - min, glm::vec3(0.0f));
				return d.x * d.y + d.y * d.z + d.z * d.x;
			};
			m_Nodes.reserve(order.size() * 2 / LEAF + 1);
			m_Nodes.push_back({});
			std::vector<Task> tasks{ { 0, 0, static_cast<uint32_t>(order.size()) } };
			while (!tasks.empty()) {
				const auto task = tasks.back();
				tasks.pop_back();
				Node node;
				glm::vec3 cmin(INF), cmax(-INF);
				for (uint32_t i = task.first; i < task.first + task.count; i++) {
					node.min = glm::min(node.min, lo[order[i]]);
					node.max = glm::max(node.max, hi[order[i]]);
					cmin = glm::min(cmin, centre[order[i]]);
					cmax = glm::max(cmax, centre[order[i]]);
				}
				node.first = task.first;
				node.count = task.count;
				int bestAxis = -1;
				int bestSplit = 0;
				float bestCost = static_cast<float>(task.count) * area(node.min, node.max); // not splitting
				if (task.count > LEAF) {
					for (int axis = 0; axis < 3; axis++) {
						const float extent = cmax[axis] - cmin[axis];
						if (!(extent > 0.0f)) continue;
						struct Bin { glm::vec3 min{ INF }, max{ -INF }; uint32_t count{}; };
						std::array<Bin, BINS> bins{};
						const float scale = BINS / extent;
						for (uint32_t i = task.first; i < task.first + task.count; i++) {
							const auto t = order[i];
							const int b = std::min(BINS - 1, static_cast<int>((centre[t][axis] - cmin[axis]) * scale));
							bins[b].min = glm::min(bins[b].min, lo[t]);
							bins[b].max = glm::max(bins[b].max, hi[t]);
							bins[b].count++;
						}
						std::array<float, BINS - 1> leftCost{};
						glm::vec3 lmin(INF), lmax(-INF);
						uint32_t lcount = 0;
						for (int b = 0; b < BINS - 1; b++) {
							lmin = glm::min(lmin, bins[b].min);
							lmax = glm::max(lmax, bins[b].max);
							lcount += bins[b].count;
							leftCost[b] = lcount ? lcount * area(lmin, lmax) : 0.0f;
						}
						glm::vec3 rmin(INF), rmax(-INF);
						uint32_t rcount = 0;
						for (int b = BINS - 1; b > 0; b--) {
							rmin = glm::min(rmin, bins[b].min);
							rmax = glm::max(rmax, bins[b].max);
							rcount += bins[b].count;
							const float cost = leftCost[b - 1] + (rcount ? rcount * area(rmin, rmax) : 0.0f);
							if (rcount > 0 && rcount < task.count && cost < bestCost) {
								bestCost = cost;
								bestAxis = axis;
								bestSplit = b;
							}
						}
					}
				}
				if (bestAxis < 0) {
					m_Nodes[task.node] = node;
					continue;
				}
				const float extent = cmax[bestAxis] - cmin[bestAxis];
				const float scale = BINS / extent;
				auto* begin = order.data() + task.first;
				auto* middle = std::partition(begin, begin + task.count, [&](uint32_t t) {
					return std::min(BINS - 1, static_cast<int>((centre[t][bestAxis] - cmin[bestAxis]) * scale)) < bestSplit;
				});
				const auto leftCount = static_cast<uint32_t>(middle - begin);
				node.first = static_cast<uint32_t>(m_Nodes.size());
				node.count = 0;
				m_Nodes[task.node] = node;
				m_Nodes.push_back({});
				m_Nodes.push_back({});
				tasks.push_back({ node.first, task.first, leftCount });
				tasks.push_back({ node.first + 1, task.first + leftCount, task.count - leftCount });
			}
		}

		std::vector<Node> m_Nodes;
		std::vector<Triangle> m_Triangles;
	};

	// LU Toolbox's ground plane: a box 1000 x 1000 x 100 whose top is at LDD y 0, black (a path hitting it ends)
	float GroundHit(const glm::vec3& origin, const glm::vec3& direction, float maxT) {
		static const glm::vec3 MIN(-500.0f, -100.0f, -500.0f), MAX(500.0f, 0.0f, 500.0f);
		float enter = 0.0f, exit = maxT;
		for (int axis = 0; axis < 3; axis++) {
			if (std::abs(direction[axis]) < 1e-20f) {
				if (origin[axis] < MIN[axis] || origin[axis] > MAX[axis]) return INF;
				continue;
			}
			const float inv = 1.0f / direction[axis];
			float t0 = (MIN[axis] - origin[axis]) * inv, t1 = (MAX[axis] - origin[axis]) * inv;
			if (t0 > t1) std::swap(t0, t1);
			enter = std::max(enter, t0);
			exit = std::min(exit, t1);
			if (enter > exit) return INF;
		}
		return enter;
	}

	class Tracer {
	public:
		Tracer(const UgcModel::Mesh& mesh, const UgcHsr::Options& options) : m_Mesh(mesh), m_Bvh(mesh), m_Options(options) {
			m_Smooth = mesh.normals.size() == mesh.positions.size();
		}

		// The triangle's normal from its winding (unit; zero when it has no area)
		glm::vec3 FaceNormal(uint32_t t) const {
			const auto& a = m_Mesh.positions[m_Mesh.indices[t * 3]];
			const auto n = glm::cross(m_Mesh.positions[m_Mesh.indices[t * 3 + 1]] - a, m_Mesh.positions[m_Mesh.indices[t * 3 + 2]] - a);
			const float length = glm::length(n);
			return length > 0.0f ? n / length : glm::vec3(0.0f);
		}

		// The vertex normals at barycentric w (Cycles' smooth normal: the face's own when they sum to nothing)
		glm::vec3 ShadingNormal(uint32_t t, const glm::vec3& w, const glm::vec3& faceNormal) const {
			if (!m_Smooth) return faceNormal;
			const auto n = m_Mesh.normals[m_Mesh.indices[t * 3]] * w.x + m_Mesh.normals[m_Mesh.indices[t * 3 + 1]] * w.y + m_Mesh.normals[m_Mesh.indices[t * 3 + 2]] * w.z;
			const float length = glm::length(n);
			return length > 0.0f ? n / length : faceNormal;
		}

		glm::vec3 Position(uint32_t t, const glm::vec3& w) const {
			return m_Mesh.positions[m_Mesh.indices[t * 3]] * w.x + m_Mesh.positions[m_Mesh.indices[t * 3 + 1]] * w.y + m_Mesh.positions[m_Mesh.indices[t * 3 + 2]] * w.z;
		}

		/**
		 * One path from point w (barycentric) of triangle t, as a Cycles diffuse bake traces it: whether it reaches the
		 * sky. The path bounces off the model in the directions the bake material draws until a bounce ray hits
		 * nothing, which is the sky. The sky isn't sampled directly: Cycles doesn't sample a world of one flat color as
		 * a light, so only rays that happen to leave count. Up to `bounces` bounces (4 of them glossy), and from the
		 * second bounce on the path may end early (Cycles' Russian roulette: it goes on with probability
		 * sqrt(throughput)).
		 */
		bool Escapes(uint32_t t, const glm::vec3& w, Random& random) const {
			glm::vec3 ng = FaceNormal(t);
			// The bake looks at the point along its smooth normal (the ray comes from there): that side is lit, and
			// when the winding faces the other way Cycles treats it as the back of the face (both normals turned)
			glm::vec3 n = ShadingNormal(t, w, ng);
			glm::vec3 incoming = n;
			if (glm::dot(ng, incoming) < 0.0f) {
				ng = -ng;
				n = -n;
			}
			glm::vec3 p = Position(t, w);
			uint32_t self = t;
			float throughput = 1.0f;
			float minRayPdf = INF;
			int glossy = 0;
			for (int bounce = 0;; bounce++) {
				// The material's normal: the Principled BSDF keeps its mirror reflection above the surface
				const Principled material(EnsureValidReflection(ng, incoming, n), incoming, ng, n, bounce == 0, minRayPdf);
				const auto sample = material.Draw(random);
				// Nothing sampled (below the surface): the path ends
				if (!(sample.pdf > 0.0f) || !(sample.throughput > 0.0f)) return false;
				const auto& direction = sample.direction;
				throughput *= sample.throughput;
				minRayPdf = std::min(minRayPdf, sample.pdf);
				const auto hit = m_Bvh.Closest(p, direction, self);
				if (m_Options.groundPlane && GroundHit(p, direction, hit.t) < hit.t) return false;
				if (hit.triangle == UINT32_MAX) return true;
				// Past the bounce limits the next surface doesn't scatter (Cycles: Max Bounces, Glossy 4)
				if (bounce + 1 > m_Options.bounces) return false;
				if (sample.glossy && ++glossy > GLOSSY_BOUNCES) return false;
				if (bounce + 1 >= 2) {
					const float probability = std::min(std::sqrt(throughput), 1.0f);
					if (random.Float() >= probability) return false;
					throughput /= probability;
				}
				self = hit.triangle;
				ng = FaceNormal(self);
				n = ShadingNormal(self, glm::vec3(1.0f - hit.u - hit.v, hit.u, hit.v), ng);
				incoming = -direction;
				// Hit from behind: the back is lit (both normals turned towards the ray)
				if (glm::dot(ng, incoming) < 0.0f) {
					ng = -ng;
					n = -n;
				}
				p = p + direction * hit.t;
			}
		}

	private:
		const UgcModel::Mesh& m_Mesh;
		Bvh m_Bvh;
		const UgcHsr::Options& m_Options;
		bool m_Smooth{};
	};
}

namespace UgcHsr {
	std::vector<glm::vec3> SamplePoints(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, float spacing, size_t minimum) {
		// Too small to lay out: the centre and one point towards each corner (the centres of its four halved-side
		// triangles)
		const std::vector<glm::vec3> fallback{ { 1.0f / 3, 1.0f / 3, 1.0f / 3 }, { 2.0f / 3, 1.0f / 6, 1.0f / 6 }, { 1.0f / 6, 2.0f / 3, 1.0f / 6 },
			{ 1.0f / 6, 1.0f / 6, 2.0f / 3 } };
		const std::array<glm::vec3, 3> corners{ a, b, c };
		// The longest side from corner `first` to the next; the third is the apex
		int first = 0;
		float longest = -1.0f;
		for (int i = 0; i < 3; i++) {
			const float length = glm::length(corners[(i + 1) % 3] - corners[i]);
			if (length > longest) {
				longest = length;
				first = i;
			}
		}
		const float area = 0.5f * glm::length(glm::cross(b - a, c - a));
		if (!(longest > 0.0f) || !(area > 0.0f) || !(spacing > 0.0f) || !std::isfinite(area)) return fallback;
		minimum = std::min(minimum, MAX_POINTS);
		// Very big triangles: points further apart, at most MAX_POINTS
		spacing = std::max(spacing, std::sqrt(area / static_cast<float>(MAX_POINTS)));
		const float height = 2.0f * area / longest;
		std::vector<glm::vec3> points;
		for (int attempt = 0; attempt < 16; attempt++) {
			points.clear();
			const int along = std::max(1, static_cast<int>(std::ceil(longest / spacing)));
			const int rows = std::max(1, static_cast<int>(std::ceil(height / spacing)));
			for (int row = 0; row < rows; row++) {
				// v: from the longest side (0) to the apex (1); the row is (1 - v) of the side's length
				const float v = (row + 0.5f) / rows;
				const int count = std::max(1, static_cast<int>(std::lround(along * (1.0f - v))));
				for (int j = 0; j < count; j++) {
					const float u = (j + 0.5f) / count;
					glm::vec3 w{};
					w[first] = (1.0f - u) * (1.0f - v);
					w[(first + 1) % 3] = u * (1.0f - v);
					w[(first + 2) % 3] = v;
					points.push_back(w);
				}
			}
			if (points.size() >= minimum) break;
			// Fewer than the minimum: closer together
			spacing *= 0.95f * std::sqrt(static_cast<float>(points.size()) / static_cast<float>(minimum));
		}
		return points.size() < fallback.size() ? fallback : points;
	}

	std::vector<bool> Visible(const UgcModel::Mesh& mesh, const Options& options, uint64_t* pointCount, uint64_t* pathCount) {
		const size_t triangles = mesh.TriangleCount();
		std::vector<bool> visible(triangles, true);
		if (triangles == 0) return visible;
		const Tracer tracer(mesh, options);
		const int samples = std::max(options.samples, 1);
		uint64_t points = 0, paths = 0, sinceCheckpoint = 0;
		for (uint32_t t = 0; t < triangles; t++) {
			const auto& a = mesh.positions[mesh.indices[t * 3]];
			const auto& b = mesh.positions[mesh.indices[t * 3 + 1]];
			const auto& c = mesh.positions[mesh.indices[t * 3 + 2]];
			// A triangle without area draws nothing (LU Toolbox's bake leaves it dark too): removed
			if (tracer.FaceNormal(t) == glm::vec3(0.0f)) {
				visible[t] = false;
				continue;
			}
			const auto weights = SamplePoints(a, b, c, options.spacing, static_cast<size_t>(std::max(options.minPoints, 1)));
			points += weights.size();
			bool escaped = false;
			// A path from each point, then another from each, ...: a triangle that's seen is usually known at once
			for (int sample = 0; sample < samples && !escaped; sample++) {
				for (size_t i = 0; i < weights.size(); i++) {
					Random random(PathSeed(options.seed, t, i, static_cast<uint64_t>(sample)));
					paths++;
					if (tracer.Escapes(t, weights[i], random)) {
						escaped = true;
						break;
					}
				}
				sinceCheckpoint += weights.size();
				if (sinceCheckpoint >= 256) {
					UgcThrottle::Checkpoint();
					sinceCheckpoint = 0;
				}
			}
			visible[t] = escaped;
		}
		if (pointCount) *pointCount = points;
		if (pathCount) *pathCount = paths;
		return visible;
	}

	Result RemoveHiddenFaces(UgcModel::Model& model, const Options& options) {
		Result result;
		auto& opaque = model.opaque;
		result.trianglesBefore = opaque.TriangleCount() + model.transparent.TriangleCount();
		if (opaque.Empty() || !options.enabled) return result;
		result.kept = Visible(opaque, options, &result.points, &result.paths);
		for (const bool kept : result.kept) result.trianglesRemoved += kept ? 0 : 1;
		UgcModel::KeepTriangles(opaque, result.kept);
		return result;
	}
}
