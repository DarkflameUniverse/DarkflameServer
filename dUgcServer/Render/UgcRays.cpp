#include "UgcRays.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>

#include <embree4/rtcore.h>

#ifdef DLU_HIPRT
#include "UgcRaysHiprt.h"
#endif

namespace {
	using UgcRays::Hit;
	using UgcRays::INF;

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
			// Further than 0 (the ray leaves a surface)
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
			// Hits exactly at minT or maxT don't count (Embree counts them)
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
		return backend == eBackend::HIPRT ? "hiprt" : "embree";
	}

	std::optional<eBackend> Parse(std::string_view name) {
		// builtin: the UGC server's own hierarchies, which Embree replaced (settings and options that name it)
		if (name == "builtin") return eBackend::EMBREE;
		for (const auto backend : { eBackend::EMBREE, eBackend::HIPRT }) {
			if (Name(backend) == name) return backend;
		}
		return std::nullopt;
	}

	bool Available(eBackend backend) {
#ifdef DLU_HIPRT
		if (backend == eBackend::HIPRT) return UgcRaysHiprt::Available();
#endif
		return backend == eBackend::EMBREE;
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
		default: return std::make_unique<EmbreeScene>(mesh);
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
