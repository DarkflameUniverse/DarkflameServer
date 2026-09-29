#include "UgcRays.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

#include <embree4/rtcore.h>

#ifdef DLU_HIPRT
#include "UgcRaysHiprt.h"
#endif

namespace {
	using UgcRays::Hit;
	using UgcRays::INF;

	/**
	 * A bounding volume hierarchy (binned surface area heuristic) over a mesh's triangles, for the paths' rays: the
	 * nearest hit. A ray never hits the triangle it leaves (`skip`), as in Cycles.
	 */
	class ClosestBvh {
	public:
		explicit ClosestBvh(const UgcModel::Mesh& mesh) {
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

	// A bounding volume hierarchy over a mesh's triangles (median splits), for the occlusion rays: any hit. The mesh
	// is only read while it is built.
	class AnyBvh {
	public:
		explicit AnyBvh(const UgcModel::Mesh& mesh) : m_Mesh(mesh) {
			const size_t count = mesh.TriangleCount();
			m_Order.resize(count);
			std::iota(m_Order.begin(), m_Order.end(), 0u);
			m_Centers.resize(count);
			for (size_t t = 0; t < count; t++) m_Centers[t] = (Vertex(t, 0) + Vertex(t, 1) + Vertex(t, 2)) / 3.0f;
			if (count > 0) Build(0, static_cast<uint32_t>(count));
			Flatten();
		}

		// Whether a ray from `origin` along `direction` (unit) hits a triangle further than `minDistance` and nearer
		// than `maxDistance`
		bool Hits(const glm::vec3& origin, const glm::vec3& direction, float minDistance, float maxDistance) const {
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
						if (TriangleHit(m_Triangles[i], origin, direction, minDistance, maxDistance)) return true;
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

		static bool TriangleHit(const Triangle& triangle, const glm::vec3& origin, const glm::vec3& direction, float minDistance, float maxDistance) {
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
			return distance > minDistance && distance < maxDistance;
		}

		const UgcModel::Mesh& m_Mesh;
		std::vector<Triangle> m_Triangles;
		std::vector<uint32_t> m_Order;
		std::vector<glm::vec3> m_Centers;
		std::vector<Node> m_Nodes;
	};

	/**
	 * builtin: the hierarchy each query had before the backends (the paths' nearest hits: ClosestBvh; the occlusion
	 * rays: AnyBvh), each built the first time it is asked, so the results are exactly what they were.
	 */
	class BuiltinScene final : public UgcRays::Scene {
	public:
		explicit BuiltinScene(const UgcModel::Mesh& mesh) {
			m_Mesh.positions = mesh.positions;
			m_Mesh.indices = mesh.indices;
		}

		Hit Closest(const glm::vec3& origin, const glm::vec3& direction, uint32_t skip, float maxT) const override {
			if (!m_Closest) m_Closest = std::make_unique<ClosestBvh>(m_Mesh);
			return m_Closest->Closest(origin, direction, skip, maxT);
		}

		bool Occluded(const glm::vec3& origin, const glm::vec3& direction, float minT, float maxT) const override {
			if (!m_Any) m_Any = std::make_unique<AnyBvh>(m_Mesh);
			return m_Any->Hits(origin, direction, minT, maxT);
		}

	private:
		UgcModel::Mesh m_Mesh; // positions and indices only
		mutable std::unique_ptr<ClosestBvh> m_Closest;
		mutable std::unique_ptr<AnyBvh> m_Any;
	};

	// Embree's device for the thread: one per thread, with no threads of its own (threads=1: the thread that commits
	// a scene builds it), released when the thread ends
	struct EmbreeDevice {
		RTCDevice device{};

		EmbreeDevice() {
			device = rtcNewDevice("threads=1,set_affinity=0,verbose=0");
			if (!device) throw std::runtime_error("Embree: no device (error " + std::to_string(rtcGetDeviceError(nullptr)) + ")");
		}
		~EmbreeDevice() { rtcReleaseDevice(device); }
		EmbreeDevice(const EmbreeDevice&) = delete;
		EmbreeDevice& operator=(const EmbreeDevice&) = delete;

		static RTCDevice Get() {
			thread_local EmbreeDevice instance;
			return instance.device;
		}
	};

	void EmbreeCheck(RTCDevice device, const char* what) {
		const auto error = rtcGetDeviceError(device);
		if (error != RTC_ERROR_NONE) throw std::runtime_error(std::string("Embree: ") + what + " failed (error " + std::to_string(error) + ")");
	}

	// A ray query's context, with the triangle the ray may not hit
	struct SkipContext {
		RTCRayQueryContext base;
		uint32_t skip;
	};

	void SkipFilter(const RTCFilterFunctionNArguments* args) {
		const auto* context = reinterpret_cast<const SkipContext*>(args->context);
		for (unsigned int i = 0; i < args->N; i++) {
			if (args->valid[i] != 0 && RTCHitN_primID(args->hit, args->N, i) == context->skip) args->valid[i] = 0;
		}
	}

	RTCRay EmbreeRay(const glm::vec3& origin, const glm::vec3& direction, float minT, float maxT) {
		RTCRay ray{};
		ray.org_x = origin.x;
		ray.org_y = origin.y;
		ray.org_z = origin.z;
		ray.dir_x = direction.x;
		ray.dir_y = direction.y;
		ray.dir_z = direction.z;
		ray.tnear = minT;
		ray.tfar = maxT;
		ray.mask = 0xFFFFFFFFu;
		ray.flags = 0;
		ray.time = 0.0f;
		return ray;
	}

	/**
	 * embree: one triangle geometry, built at high quality and traced watertight (a ray through the edge two
	 * triangles share hits one of them). The triangle a path leaves is skipped by a filter function.
	 */
	class EmbreeScene final : public UgcRays::Scene {
	public:
		explicit EmbreeScene(const UgcModel::Mesh& mesh) {
			const auto device = EmbreeDevice::Get();
			const size_t triangles = mesh.TriangleCount();
			m_Scene = rtcNewScene(device);
			EmbreeCheck(device, "rtcNewScene");
			rtcSetSceneBuildQuality(m_Scene, RTC_BUILD_QUALITY_HIGH);
			rtcSetSceneFlags(m_Scene, RTC_SCENE_FLAG_ROBUST | RTC_SCENE_FLAG_FILTER_FUNCTION_IN_ARGUMENTS);
			if (triangles > 0 && !mesh.positions.empty()) {
				const RTCGeometry geometry = rtcNewGeometry(device, RTC_GEOMETRY_TYPE_TRIANGLE);
				// Copied into Embree's own buffers, which are padded for its vector loads (a std::vector isn't)
				auto* vertices = static_cast<float*>(rtcSetNewGeometryBuffer(geometry, RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3, sizeof(float) * 3, mesh.positions.size()));
				auto* indices = static_cast<uint32_t*>(rtcSetNewGeometryBuffer(geometry, RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT3, sizeof(uint32_t) * 3, triangles));
				if (!vertices || !indices) {
					rtcReleaseGeometry(geometry);
					EmbreeCheck(device, "rtcSetNewGeometryBuffer");
					throw std::runtime_error("Embree: no geometry buffers");
				}
				for (size_t i = 0; i < mesh.positions.size(); i++) {
					vertices[i * 3] = mesh.positions[i].x;
					vertices[i * 3 + 1] = mesh.positions[i].y;
					vertices[i * 3 + 2] = mesh.positions[i].z;
				}
				std::memcpy(indices, mesh.indices.data(), sizeof(uint32_t) * 3 * triangles);
				rtcSetGeometryEnableFilterFunctionFromArguments(geometry, true);
				rtcCommitGeometry(geometry);
				rtcAttachGeometry(m_Scene, geometry);
				rtcReleaseGeometry(geometry);
				m_Empty = false;
			}
			rtcCommitScene(m_Scene);
			EmbreeCheck(device, "rtcCommitScene");
		}

		~EmbreeScene() override {
			if (m_Scene) rtcReleaseScene(m_Scene);
		}

		EmbreeScene(const EmbreeScene&) = delete;
		EmbreeScene& operator=(const EmbreeScene&) = delete;

		Hit Closest(const glm::vec3& origin, const glm::vec3& direction, uint32_t skip, float maxT) const override {
			Hit hit;
			hit.t = maxT;
			if (m_Empty) return hit;
			RTCRayHit query{};
			// Further than 0, as the builtin test
			query.ray = EmbreeRay(origin, direction, std::numeric_limits<float>::min(), maxT);
			query.hit.geomID = RTC_INVALID_GEOMETRY_ID;
			query.hit.primID = RTC_INVALID_GEOMETRY_ID;
			SkipContext context{};
			rtcInitRayQueryContext(&context.base);
			context.skip = skip;
			RTCIntersectArguments arguments;
			rtcInitIntersectArguments(&arguments);
			arguments.context = &context.base;
			if (skip != UgcRays::NONE) {
				arguments.filter = SkipFilter;
				arguments.flags = static_cast<RTCRayQueryFlags>(arguments.flags | RTC_RAY_QUERY_FLAG_INVOKE_ARGUMENT_FILTER);
			}
			rtcIntersect1(m_Scene, &query, &arguments);
			if (query.hit.geomID == RTC_INVALID_GEOMETRY_ID) return hit;
			hit.t = query.ray.tfar;
			hit.triangle = query.hit.primID;
			hit.u = query.hit.u;
			hit.v = query.hit.v;
			return hit;
		}

		bool Occluded(const glm::vec3& origin, const glm::vec3& direction, float minT, float maxT) const override {
			if (m_Empty) return false;
			// Embree counts hits at exactly tnear and tfar; the builtin test doesn't
			auto ray = EmbreeRay(origin, direction, std::nextafter(minT, INF), std::nextafter(maxT, 0.0f));
			rtcOccluded1(m_Scene, &ray, nullptr);
			return ray.tfar < 0.0f;
		}

	private:
		RTCScene m_Scene{};
		bool m_Empty{ true };
	};
}

namespace UgcRays {
	void Scene::Closest(const Ray* rays, Hit* hits, size_t count) const {
		for (size_t i = 0; i < count; i++) hits[i] = Closest(rays[i].origin, rays[i].direction, rays[i].skip, rays[i].maxT);
	}

	void Scene::Occluded(const Ray* rays, uint8_t* occluded, size_t count) const {
		for (size_t i = 0; i < count; i++) occluded[i] = Occluded(rays[i].origin, rays[i].direction, rays[i].minT, rays[i].maxT) ? 1 : 0;
	}

	std::string_view Name(eBackend backend) {
		switch (backend) {
		case eBackend::EMBREE: return "embree";
		case eBackend::HIPRT: return "hiprt";
		default: return "builtin";
		}
	}

	std::optional<eBackend> Parse(std::string_view name) {
		for (const auto backend : { eBackend::BUILTIN, eBackend::EMBREE, eBackend::HIPRT }) {
			if (Name(backend) == name) return backend;
		}
		return std::nullopt;
	}

	bool Available(eBackend backend) {
#ifdef DLU_HIPRT
		if (backend == eBackend::HIPRT) return UgcRaysHiprt::Available();
#endif
		return backend == eBackend::BUILTIN || backend == eBackend::EMBREE;
	}

	eBackend Resolve(eBackend wanted) {
		return Available(wanted) ? wanted : eBackend::EMBREE;
	}

	std::string Problem(eBackend backend) {
		if (Available(backend)) return {};
#ifdef DLU_HIPRT
		if (backend == eBackend::HIPRT) return UgcRaysHiprt::Problem();
#endif
		return "the server was built without it (DLU_HIPRT)";
	}

	std::unique_ptr<Scene> Make(eBackend backend, const UgcModel::Mesh& mesh) {
		switch (Resolve(backend)) {
#ifdef DLU_HIPRT
		case eBackend::HIPRT:
			// A GPU that fails now (out of memory, ...) leaves the job to Embree
			if (auto scene = UgcRaysHiprt::Make(mesh)) return scene;
			return std::make_unique<EmbreeScene>(mesh);
#endif
		case eBackend::EMBREE: return std::make_unique<EmbreeScene>(mesh);
		default: return std::make_unique<BuiltinScene>(mesh);
		}
	}

	void SetGpuDevice(int index) {
#ifdef DLU_HIPRT
		UgcRaysHiprt::SetDevice(index);
#else
		(void)index;
#endif
	}
}
