#include "UgcRaysHiprt.h"

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <mutex>
#include <stdexcept>
#include <vector>

#include <Orochi/Orochi.h>

// hiprtew.h loads HIPRT's library when asked (hiprtewInit) instead of linking it; its function pointers live here
#ifndef _WIN32
#define HIPRTAPI
#endif
#define _ENABLE_HIPRTEW
#include <hiprt/hiprtew.h>

#include "BinaryPathFinder.h"

namespace {
	// The trace kernels, compiled at run time by HIPRT (for the GPU found). A ray is 48 bytes and a hit 16, as
	// UgcRays::Ray and UgcRays::Hit. Closest never returns the ray's `skip` triangle: when it is the nearest, the
	// ray goes on from just past it (a few times at most).
	const char* KERNELS = R"(
#include <hiprt/hiprt_device.h>

struct UgcRay { float ox, oy, oz, minT, dx, dy, dz, maxT; unsigned skip, pad0, pad1, pad2; };
struct UgcHit { float t; unsigned triangle; float u, v; };

extern "C" __global__ void UgcClosest(hiprtGeometry geometry, const UgcRay* rays, UgcHit* hits, unsigned count) {
	const unsigned i = blockIdx.x * blockDim.x + threadIdx.x;
	if (i >= count) return;
	const UgcRay r = rays[i];
	hiprtRay ray;
	ray.origin = { r.ox, r.oy, r.oz };
	ray.direction = { r.dx, r.dy, r.dz };
	ray.minT = 1.17549435e-38f;
	// Finite: a ray grazing a triangle can otherwise hit it at infinity
	ray.maxT = fminf(r.maxT, 3.40282347e38f);
	UgcHit out = { r.maxT, 0xFFFFFFFFu, 0.0f, 0.0f };
	for (int attempt = 0; attempt < 8; attempt++) {
		hiprtGeomTraversalClosest traversal(geometry, ray);
		const hiprtHit hit = traversal.getNextHit();
		if (!hit.hasHit()) break;
		if (hit.primID != r.skip) {
			out.t = hit.t;
			out.triangle = hit.primID;
			out.u = hit.uv.x;
			out.v = hit.uv.y;
			break;
		}
		ray.minT = nextafterf(hit.t, 3.4e38f);
	}
	hits[i] = out;
}

extern "C" __global__ void UgcOccluded(hiprtGeometry geometry, const UgcRay* rays, unsigned char* occluded, unsigned count) {
	const unsigned i = blockIdx.x * blockDim.x + threadIdx.x;
	if (i >= count) return;
	const UgcRay r = rays[i];
	hiprtRay ray;
	ray.origin = { r.ox, r.oy, r.oz };
	ray.direction = { r.dx, r.dy, r.dz };
	// Hits exactly at minT or maxT don't count, as in the other backends
	ray.minT = nextafterf(r.minT, 3.4e38f);
	ray.maxT = nextafterf(fminf(r.maxT, 3.40282347e38f), 0.0f);
	hiprtGeomTraversalAnyHit traversal(geometry, ray);
	occluded[i] = traversal.getNextHit().hasHit() ? 1 : 0;
}
)";

	// Rays sent to the GPU at once at most (48 MB of rays)
	constexpr size_t BATCH = 1u << 20;
	constexpr unsigned BLOCK = 64;

	std::atomic<int> g_DeviceIndex{ 0 };
	std::mutex g_Mutex; // one thread on the GPU at a time; everything below is used under it

	struct Gpu {
		bool tried{};
		bool ok{};
		std::string problem;
		oroDevice device{};
		oroCtx context{};
		hiprtContext rt{};
		hiprtApiFunction closest{};
		hiprtApiFunction occluded{};
		void* rays{};   // device buffers for a batch, grown as needed
		void* results{};
		size_t capacity{};
	} g_Gpu;

	std::string OroMessage(oroError error) {
		const char* text = nullptr;
		oroGetErrorString(error, &text);
		return text ? text : "error " + std::to_string(static_cast<int>(error));
	}

	void Check(oroError error, const char* what) {
		if (error != oroSuccess) throw std::runtime_error(std::string("GPU: ") + what + " failed (" + OroMessage(error) + ")");
	}

	void Check(hiprtError error, const char* what) {
		if (error != hiprtSuccess) throw std::runtime_error(std::string("HIPRT: ") + what + " failed (error " + std::to_string(static_cast<int>(error)) + ")");
	}

	// Where the HIPRT headers the kernels include are: next to the servers (copied there by the build), else where
	// the build found them
	std::string IncludeFolder() {
		const auto local = BinaryPathFinder::GetBinaryDir() / "hiprt" / "include";
		if (std::filesystem::exists(local / "hiprt" / "hiprt_device.h")) return local.string();
		return DLU_HIPRT_INCLUDE_DIR;
	}

	// Under g_Mutex: loads HIP or CUDA and HIPRT, makes the context and compiles the kernels, once
	bool Init() {
		if (g_Gpu.tried) return g_Gpu.ok;
		g_Gpu.tried = true;
		try {
			if (oroInitialize(static_cast<oroApi>(ORO_API_HIP | ORO_API_CUDA), 0) != 0) throw std::runtime_error("neither HIP nor CUDA could be loaded");
			Check(oroInit(0), "oroInit");
			int count = 0;
			Check(oroGetDeviceCount(&count), "oroGetDeviceCount");
			const int index = g_DeviceIndex;
			if (index < 0 || index >= count) throw std::runtime_error("no GPU " + std::to_string(index) + " (" + std::to_string(count) + " found)");
			Check(oroDeviceGet(&g_Gpu.device, index), "oroDeviceGet");
			Check(oroCtxCreate(&g_Gpu.context, 0, g_Gpu.device), "oroCtxCreate");
			int loaded = 0;
			hiprtewInit(&loaded);
			if (loaded != HIPRTEW_SUCCESS) throw std::runtime_error(std::string("the HIPRT library (") + HIPRT_LIB_NAME + ") could not be loaded");
			hiprtContextCreationInput input{};
			input.ctxt = oroGetRawCtx(g_Gpu.context);
			input.device = oroGetRawDevice(g_Gpu.device);
			input.deviceType = oroGetCurAPI(0) == ORO_API_CUDADRIVER ? hiprtDeviceNVIDIA : hiprtDeviceAMD;
			Check(hiprtCreateContext(HIPRT_API_VERSION, input, g_Gpu.rt), "hiprtCreateContext");
			const auto cache = BinaryPathFinder::GetBinaryDir() / "cache" / "hiprt";
			std::error_code error;
			std::filesystem::create_directories(cache, error);
			if (!error) hiprtSetCacheDirPath(g_Gpu.rt, cache.string().c_str());
			const char* names[] = { "UgcClosest", "UgcOccluded" };
			hiprtApiFunction functions[2]{};
			const auto include = "-I" + IncludeFolder();
			const char* options[] = { include.c_str() };
			Check(hiprtBuildTraceKernels(g_Gpu.rt, 2, names, KERNELS, "UgcRays", 0, nullptr, nullptr, 1, options, 0, 1, nullptr, functions, nullptr, true),
				"hiprtBuildTraceKernels");
			g_Gpu.closest = functions[0];
			g_Gpu.occluded = functions[1];
			g_Gpu.ok = true;
		} catch (const std::exception& ex) {
			g_Gpu.problem = ex.what();
			g_Gpu.ok = false;
		}
		return g_Gpu.ok;
	}

	// Under g_Mutex, with the context current: device buffers for `count` rays and their results
	void Reserve(size_t count) {
		if (count <= g_Gpu.capacity) return;
		if (g_Gpu.rays) oroFree(g_Gpu.rays);
		if (g_Gpu.results) oroFree(g_Gpu.results);
		g_Gpu.rays = g_Gpu.results = nullptr;
		g_Gpu.capacity = 0;
		Check(oroMalloc(&g_Gpu.rays, count * sizeof(UgcRays::Ray)), "oroMalloc");
		Check(oroMalloc(&g_Gpu.results, count * sizeof(UgcRays::Hit)), "oroMalloc");
		g_Gpu.capacity = count;
	}

	class HiprtScene final : public UgcRays::Scene {
	public:
		explicit HiprtScene(const UgcModel::Mesh& mesh) {
			m_Triangles = static_cast<uint32_t>(mesh.TriangleCount());
			if (m_Triangles == 0 || mesh.positions.empty()) return;
			std::lock_guard lock(g_Mutex);
			Check(oroCtxSetCurrent(g_Gpu.context), "oroCtxSetCurrent");
			try {
				const size_t vertexBytes = mesh.positions.size() * sizeof(glm::vec3), indexBytes = static_cast<size_t>(m_Triangles) * 3 * sizeof(uint32_t);
				Check(oroMalloc(&m_Vertices, vertexBytes), "oroMalloc");
				Check(oroMalloc(&m_Indices, indexBytes), "oroMalloc");
				Check(oroMemcpyHtoD(reinterpret_cast<oroDeviceptr>(m_Vertices), const_cast<glm::vec3*>(mesh.positions.data()), vertexBytes), "oroMemcpyHtoD");
				Check(oroMemcpyHtoD(reinterpret_cast<oroDeviceptr>(m_Indices), const_cast<uint32_t*>(mesh.indices.data()), indexBytes), "oroMemcpyHtoD");
				hiprtTriangleMeshPrimitive triangles{};
				triangles.vertices = m_Vertices;
				triangles.vertexCount = static_cast<uint32_t>(mesh.positions.size());
				triangles.vertexStride = sizeof(glm::vec3);
				triangles.triangleIndices = m_Indices;
				triangles.triangleCount = m_Triangles;
				triangles.triangleStride = 3 * sizeof(uint32_t);
				hiprtGeometryBuildInput input{};
				input.type = hiprtPrimitiveTypeTriangleMesh;
				input.primitive.triangleMesh = triangles;
				hiprtBuildOptions options{};
				options.buildFlags = hiprtBuildFlagBitPreferHighQualityBuild;
				size_t temporaryBytes = 0;
				Check(hiprtGetGeometryBuildTemporaryBufferSize(g_Gpu.rt, input, options, temporaryBytes), "hiprtGetGeometryBuildTemporaryBufferSize");
				void* temporary = nullptr;
				if (temporaryBytes > 0) Check(oroMalloc(&temporary, temporaryBytes), "oroMalloc");
				Check(hiprtCreateGeometry(g_Gpu.rt, input, options, m_Geometry), "hiprtCreateGeometry");
				m_Created = true;
				const auto built = hiprtBuildGeometry(g_Gpu.rt, hiprtBuildOperationBuild, input, options, temporary, nullptr, m_Geometry);
				if (temporary) oroFree(temporary);
				Check(built, "hiprtBuildGeometry");
			} catch (...) {
				Release();
				throw;
			}
		}

		~HiprtScene() override {
			std::lock_guard lock(g_Mutex);
			if (oroCtxSetCurrent(g_Gpu.context) == oroSuccess) Release();
		}

		HiprtScene(const HiprtScene&) = delete;
		HiprtScene& operator=(const HiprtScene&) = delete;

		UgcRays::Hit Closest(const glm::vec3& origin, const glm::vec3& direction, uint32_t skip, float maxT) const override {
			UgcRays::Ray ray{ origin, 0.0f, direction, maxT, skip };
			UgcRays::Hit hit;
			Closest(&ray, &hit, 1);
			return hit;
		}

		bool Occluded(const glm::vec3& origin, const glm::vec3& direction, float minT, float maxT) const override {
			UgcRays::Ray ray{ origin, minT, direction, maxT };
			uint8_t occluded = 0;
			Occluded(&ray, &occluded, 1);
			return occluded != 0;
		}

		void Closest(const UgcRays::Ray* rays, UgcRays::Hit* hits, size_t count) const override {
			if (m_Triangles == 0) {
				for (size_t i = 0; i < count; i++) hits[i] = UgcRays::Hit{ rays[i].maxT };
				return;
			}
			Trace(g_Gpu.closest, rays, hits, sizeof(UgcRays::Hit), count);
		}

		void Occluded(const UgcRays::Ray* rays, uint8_t* occluded, size_t count) const override {
			if (m_Triangles == 0) {
				std::fill(occluded, occluded + count, uint8_t{ 0 });
				return;
			}
			Trace(g_Gpu.occluded, rays, occluded, sizeof(uint8_t), count);
		}

		bool PrefersBatches() const override { return true; }

	private:
		// The rays through `function` in batches; `results` gets `resultBytes` a ray
		void Trace(hiprtApiFunction function, const UgcRays::Ray* rays, void* results, size_t resultBytes, size_t count) const {
			std::lock_guard lock(g_Mutex);
			Check(oroCtxSetCurrent(g_Gpu.context), "oroCtxSetCurrent");
			Reserve(std::min(count, BATCH));
			for (size_t first = 0; first < count; first += BATCH) {
				const auto batch = static_cast<unsigned>(std::min(BATCH, count - first));
				Check(oroMemcpyHtoD(reinterpret_cast<oroDeviceptr>(g_Gpu.rays), const_cast<UgcRays::Ray*>(rays + first), batch * sizeof(UgcRays::Ray)), "oroMemcpyHtoD");
				hiprtGeometry geometry = m_Geometry;
				void* deviceRays = g_Gpu.rays;
				void* deviceResults = g_Gpu.results;
				unsigned batchCount = batch;
				void* arguments[] = { &geometry, &deviceRays, &deviceResults, &batchCount };
				Check(oroModuleLaunchKernel(reinterpret_cast<oroFunction>(function), (batch + BLOCK - 1) / BLOCK, 1, 1, BLOCK, 1, 1, 0, nullptr, arguments, nullptr),
					"oroModuleLaunchKernel");
				Check(oroDeviceSynchronize(), "oroDeviceSynchronize");
				Check(oroMemcpyDtoH(static_cast<uint8_t*>(results) + first * resultBytes, reinterpret_cast<oroDeviceptr>(g_Gpu.results), batch * resultBytes), "oroMemcpyDtoH");
			}
		}

		// Under g_Mutex with the context current
		void Release() {
			if (m_Created) hiprtDestroyGeometry(g_Gpu.rt, m_Geometry);
			if (m_Vertices) oroFree(m_Vertices);
			if (m_Indices) oroFree(m_Indices);
			m_Created = false;
			m_Vertices = m_Indices = nullptr;
		}

		uint32_t m_Triangles{};
		void* m_Vertices{};
		void* m_Indices{};
		hiprtGeometry m_Geometry{};
		bool m_Created{};
	};
}

namespace UgcRaysHiprt {
	bool Available() {
		std::lock_guard lock(g_Mutex);
		return Init();
	}

	std::string Problem() {
		std::lock_guard lock(g_Mutex);
		return g_Gpu.problem;
	}

	std::unique_ptr<UgcRays::Scene> Make(const UgcModel::Mesh& mesh) {
		if (!Available()) return nullptr;
		try {
			return std::make_unique<HiprtScene>(mesh);
		} catch (const std::exception&) {
			return nullptr;
		}
	}

	void SetDevice(int index) {
		g_DeviceIndex = index;
	}
}
