// Embree 4 on an Intel GPU through SYCL, behind the plain C interface in UgcEmbreeSycl.h. Compiled by a SYCL compiler
// (Intel oneAPI DPC++ or the open source DPC++) into libdlu_embree_sycl, which the UGC server loads when asked for it.

#include "UgcEmbreeSycl.h"

#include <sycl/sycl.hpp>
#include <embree4/rtcore.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#if defined(RTC_NAMESPACE_USE)
RTC_NAMESPACE_USE
#endif

namespace {
	// The ray and hit layouts the UGC server sends (UgcRays::Ray, UgcRays::Hit)
	struct Ray {
		float ox, oy, oz, minT, dx, dy, dz, maxT;
		uint32_t skip, pad0, pad1, pad2;
	};
	struct Hit {
		float t;
		uint32_t triangle;
		float u, v;
	};
	static_assert(sizeof(Ray) == 48 && sizeof(Hit) == 16, "the UGC server's layouts");

	constexpr uint32_t NONE = 0xFFFFFFFFu;
	constexpr float FLOAT_MAX = 3.40282347e38f;

	// Triangles only: the kernels are specialized for them
	const sycl::specialization_id<RTCFeatureFlags> FEATURES;
	constexpr RTCFeatureFlags REQUIRED = RTC_FEATURE_FLAG_TRIANGLE;

	struct Gpu {
		sycl::device device;
		sycl::context context;
		std::unique_ptr<sycl::queue> queue;
		RTCDevice rtc{};
		// Shared (USM) buffers for a batch, grown as needed
		Ray* rays{};
		void* results{};
		size_t capacity{};
	};
	Gpu* g_Gpu{};

	void Reserve(size_t count) {
		if (count <= g_Gpu->capacity) return;
		if (g_Gpu->rays) sycl::free(g_Gpu->rays, *g_Gpu->queue);
		if (g_Gpu->results) sycl::free(g_Gpu->results, *g_Gpu->queue);
		g_Gpu->rays = sycl::malloc_shared<Ray>(count, *g_Gpu->queue);
		g_Gpu->results = sycl::malloc_shared(count * sizeof(Hit), *g_Gpu->queue);
		g_Gpu->capacity = g_Gpu->rays && g_Gpu->results ? count : 0;
	}

	struct Scene {
		RTCScene scene{};
		RTCTraversable traversable{};
	};
}

extern "C" {
	int DluEmbreeSyclInit(int index, char* problem, int problemSize) {
		const auto fail = [&](const std::string& why) {
			if (problem && problemSize > 0) std::snprintf(problem, static_cast<size_t>(problemSize), "%s", why.c_str());
			return 1;
		};
		if (g_Gpu) return 0;
		try {
			std::vector<sycl::device> supported;
			for (const auto& device : sycl::device::get_devices(sycl::info::device_type::gpu)) {
				if (rtcIsSYCLDeviceSupported(device)) supported.push_back(device);
			}
			if (supported.empty()) return fail("no Intel GPU Embree supports (Arc or Xe, with its compute runtime)");
			if (index < 0 || index >= static_cast<int>(supported.size())) {
				return fail("no supported GPU " + std::to_string(index) + " (" + std::to_string(supported.size()) + " found)");
			}
			auto gpu = std::make_unique<Gpu>();
			gpu->device = supported[static_cast<size_t>(index)];
			gpu->context = sycl::context(gpu->device);
			gpu->queue = std::make_unique<sycl::queue>(gpu->context, gpu->device, sycl::property::queue::in_order());
			gpu->rtc = rtcNewSYCLDevice(gpu->context, "");
			if (!gpu->rtc) return fail(std::string("Embree: no SYCL device (") + rtcGetDeviceLastErrorMessage(nullptr) + ")");
			rtcSetDeviceSYCLDevice(gpu->rtc, gpu->device);
			g_Gpu = gpu.release();
			return 0;
		} catch (const std::exception& ex) {
			return fail(std::string("SYCL: ") + ex.what());
		}
	}

	void* DluEmbreeSyclScene(const float* positions, uint32_t vertices, const uint32_t* indices, uint32_t triangles) {
		if (!g_Gpu) return nullptr;
		auto scene = std::make_unique<Scene>();
		scene->scene = rtcNewScene(g_Gpu->rtc);
		if (!scene->scene) return nullptr;
		rtcSetSceneBuildQuality(scene->scene, RTC_BUILD_QUALITY_HIGH);
		rtcSetSceneFlags(scene->scene, RTC_SCENE_FLAG_ROBUST);
		if (triangles > 0 && vertices > 0) {
			const RTCGeometry geometry = rtcNewGeometry(g_Gpu->rtc, RTC_GEOMETRY_TYPE_TRIANGLE);
			// Embree's own buffers (USM the GPU reads)
			auto* v = static_cast<float*>(rtcSetNewGeometryBuffer(geometry, RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3, 3 * sizeof(float), vertices));
			auto* i = static_cast<uint32_t*>(rtcSetNewGeometryBuffer(geometry, RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT3, 3 * sizeof(uint32_t), triangles));
			if (!v || !i) {
				rtcReleaseGeometry(geometry);
				rtcReleaseScene(scene->scene);
				return nullptr;
			}
			std::memcpy(v, positions, sizeof(float) * 3 * vertices);
			std::memcpy(i, indices, sizeof(uint32_t) * 3 * triangles);
			rtcCommitGeometry(geometry);
			rtcAttachGeometry(scene->scene, geometry);
			rtcReleaseGeometry(geometry);
		}
		rtcCommitScene(scene->scene);
		if (rtcGetDeviceError(g_Gpu->rtc) != RTC_ERROR_NONE) {
			rtcReleaseScene(scene->scene);
			return nullptr;
		}
		scene->traversable = rtcGetSceneTraversable(scene->scene);
		return scene.release();
	}

	void DluEmbreeSyclRelease(void* handle) {
		auto* scene = static_cast<Scene*>(handle);
		if (!scene) return;
		if (scene->scene) rtcReleaseScene(scene->scene);
		delete scene;
	}

	int DluEmbreeSyclClosest(void* handle, const void* rays, void* hits, uint32_t count) {
		auto* scene = static_cast<Scene*>(handle);
		if (!g_Gpu || !scene) return 1;
		try {
			Reserve(count);
			if (g_Gpu->capacity < count) return 1;
			std::memcpy(g_Gpu->rays, rays, count * sizeof(Ray));
			const Ray* in = g_Gpu->rays;
			Hit* out = static_cast<Hit*>(g_Gpu->results);
			const RTCTraversable traversable = scene->traversable;
			g_Gpu->queue->submit([=](sycl::handler& handler) {
				handler.set_specialization_constant<FEATURES>(REQUIRED);
				handler.parallel_for(sycl::range<1>(count), [=](sycl::item<1> item, sycl::kernel_handler kernel) {
					const size_t k = item.get_id(0);
					const Ray r = in[k];
					Hit hit{ r.maxT, NONE, 0.0f, 0.0f };
					float minT = 1.17549435e-38f;
					// The ray's skip triangle is never hit: when it is the nearest, the ray goes on from just past it
					for (int attempt = 0; attempt < 8; attempt++) {
						RTCRayHit query{};
						query.ray.org_x = r.ox;
						query.ray.org_y = r.oy;
						query.ray.org_z = r.oz;
						query.ray.dir_x = r.dx;
						query.ray.dir_y = r.dy;
						query.ray.dir_z = r.dz;
						query.ray.tnear = minT;
						query.ray.tfar = sycl::fmin(r.maxT, FLOAT_MAX);
						query.ray.mask = 0xFFFFFFFFu;
						query.hit.geomID = RTC_INVALID_GEOMETRY_ID;
						query.hit.primID = RTC_INVALID_GEOMETRY_ID;
						RTCIntersectArguments arguments;
						rtcInitIntersectArguments(&arguments);
						arguments.feature_mask = kernel.get_specialization_constant<FEATURES>();
						rtcTraversableIntersect1(traversable, &query, &arguments);
						if (query.hit.geomID == RTC_INVALID_GEOMETRY_ID) break;
						if (query.hit.primID != r.skip) {
							hit = Hit{ query.ray.tfar, query.hit.primID, query.hit.u, query.hit.v };
							break;
						}
						minT = sycl::nextafter(query.ray.tfar, FLOAT_MAX);
					}
					out[k] = hit;
				});
			});
			g_Gpu->queue->wait_and_throw();
			std::memcpy(hits, out, count * sizeof(Hit));
			return 0;
		} catch (const std::exception&) {
			return 1;
		}
	}

	int DluEmbreeSyclOccluded(void* handle, const void* rays, uint8_t* occluded, uint32_t count) {
		auto* scene = static_cast<Scene*>(handle);
		if (!g_Gpu || !scene) return 1;
		try {
			Reserve(count);
			if (g_Gpu->capacity < count) return 1;
			std::memcpy(g_Gpu->rays, rays, count * sizeof(Ray));
			const Ray* in = g_Gpu->rays;
			uint8_t* out = static_cast<uint8_t*>(g_Gpu->results);
			const RTCTraversable traversable = scene->traversable;
			g_Gpu->queue->submit([=](sycl::handler& handler) {
				handler.set_specialization_constant<FEATURES>(REQUIRED);
				handler.parallel_for(sycl::range<1>(count), [=](sycl::item<1> item, sycl::kernel_handler kernel) {
					const size_t k = item.get_id(0);
					const Ray r = in[k];
					RTCRay ray{};
					ray.org_x = r.ox;
					ray.org_y = r.oy;
					ray.org_z = r.oz;
					ray.dir_x = r.dx;
					ray.dir_y = r.dy;
					ray.dir_z = r.dz;
					// Hits exactly at minT or maxT don't count, as in the other backends
					ray.tnear = sycl::nextafter(r.minT, FLOAT_MAX);
					ray.tfar = sycl::nextafter(sycl::fmin(r.maxT, FLOAT_MAX), 0.0f);
					ray.mask = 0xFFFFFFFFu;
					RTCOccludedArguments arguments;
					rtcInitOccludedArguments(&arguments);
					arguments.feature_mask = kernel.get_specialization_constant<FEATURES>();
					rtcTraversableOccluded1(traversable, &ray, &arguments);
					out[k] = ray.tfar < 0.0f ? 1 : 0;
				});
			});
			g_Gpu->queue->wait_and_throw();
			std::memcpy(occluded, out, count);
			return 0;
		} catch (const std::exception&) {
			return 1;
		}
	}
}
