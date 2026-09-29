#include "UgcRaysEmbreeGpu.h"

#include <algorithm>
#include <atomic>
#include <mutex>
#include <stdexcept>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include "BinaryPathFinder.h"
#include "EmbreeSycl/UgcEmbreeSycl.h"

namespace {
#if defined(_WIN32)
	constexpr const char* LIBRARY = "dlu_embree_sycl.dll";
#else
	constexpr const char* LIBRARY = "libdlu_embree_sycl.so";
#endif
	// Rays sent to the GPU at once at most (48 MB of rays)
	constexpr size_t BATCH = 1u << 20;

	std::atomic<int> g_DeviceIndex{ 0 };
	std::mutex g_Mutex; // one thread on the GPU at a time; everything below is used under it

	struct Library {
		bool tried{};
		bool ok{};
		std::string problem;
		decltype(&DluEmbreeSyclInit) init{};
		decltype(&DluEmbreeSyclScene) scene{};
		decltype(&DluEmbreeSyclRelease) release{};
		decltype(&DluEmbreeSyclClosest) closest{};
		decltype(&DluEmbreeSyclOccluded) occluded{};
	} g_Library;

	template<typename T>
	bool Find(void* handle, const char* name, T& out) {
#if defined(_WIN32)
		out = reinterpret_cast<T>(GetProcAddress(static_cast<HMODULE>(handle), name));
#else
		out = reinterpret_cast<T>(dlsym(handle, name));
#endif
		return out != nullptr;
	}

	// Under g_Mutex: loads the library next to the servers and sets the GPU up, once
	bool Init() {
		if (g_Library.tried) return g_Library.ok;
		g_Library.tried = true;
		const auto path = (BinaryPathFinder::GetBinaryDir() / LIBRARY).string();
#if defined(_WIN32)
		void* handle = LoadLibraryA(path.c_str());
#else
		// Local: its symbols (Embree's among them, bound inside it) stay its own
		void* handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
#endif
		if (!handle) {
#if defined(_WIN32)
			g_Library.problem = path + " could not be loaded";
#else
			const char* error = dlerror();
			g_Library.problem = path + " could not be loaded (" + (error ? error : "?") + ")";
#endif
			return false;
		}
		if (!Find(handle, "DluEmbreeSyclInit", g_Library.init) || !Find(handle, "DluEmbreeSyclScene", g_Library.scene) ||
			!Find(handle, "DluEmbreeSyclRelease", g_Library.release) || !Find(handle, "DluEmbreeSyclClosest", g_Library.closest) ||
			!Find(handle, "DluEmbreeSyclOccluded", g_Library.occluded)) {
			g_Library.problem = path + " is not the UGC server's (functions missing)";
			return false;
		}
		char problem[512]{};
		if (g_Library.init(g_DeviceIndex, problem, sizeof(problem)) != 0) {
			g_Library.problem = problem;
			return false;
		}
		g_Library.ok = true;
		return true;
	}

	class EmbreeGpuScene final : public UgcRays::Scene {
	public:
		explicit EmbreeGpuScene(const UgcModel::Mesh& mesh) {
			std::lock_guard lock(g_Mutex);
			m_Scene = g_Library.scene(reinterpret_cast<const float*>(mesh.positions.data()), static_cast<uint32_t>(mesh.positions.size()), mesh.indices.data(),
				static_cast<uint32_t>(mesh.TriangleCount()));
			if (!m_Scene) throw std::runtime_error("Embree (GPU): the scene could not be made");
		}

		~EmbreeGpuScene() override {
			std::lock_guard lock(g_Mutex);
			g_Library.release(m_Scene);
		}

		EmbreeGpuScene(const EmbreeGpuScene&) = delete;
		EmbreeGpuScene& operator=(const EmbreeGpuScene&) = delete;

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
			std::lock_guard lock(g_Mutex);
			for (size_t first = 0; first < count; first += BATCH) {
				const auto batch = static_cast<uint32_t>(std::min(BATCH, count - first));
				if (g_Library.closest(m_Scene, rays + first, hits + first, batch) != 0) throw std::runtime_error("Embree (GPU): tracing failed");
			}
		}

		void Occluded(const UgcRays::Ray* rays, uint8_t* occluded, size_t count) const override {
			std::lock_guard lock(g_Mutex);
			for (size_t first = 0; first < count; first += BATCH) {
				const auto batch = static_cast<uint32_t>(std::min(BATCH, count - first));
				if (g_Library.occluded(m_Scene, rays + first, occluded + first, batch) != 0) throw std::runtime_error("Embree (GPU): tracing failed");
			}
		}

		bool PrefersBatches() const override { return true; }

	private:
		void* m_Scene{};
	};
}

namespace UgcRaysEmbreeGpu {
	bool Available() {
		std::lock_guard lock(g_Mutex);
		return Init();
	}

	std::string Problem() {
		std::lock_guard lock(g_Mutex);
		return g_Library.problem;
	}

	std::unique_ptr<UgcRays::Scene> Make(const UgcModel::Mesh& mesh) {
		if (!Available()) return nullptr;
		try {
			return std::make_unique<EmbreeGpuScene>(mesh);
		} catch (const std::exception&) {
			return nullptr;
		}
	}

	void SetDevice(int index) {
		g_DeviceIndex = index;
	}
}
