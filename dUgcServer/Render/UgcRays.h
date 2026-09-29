#pragma once

#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include <glm/glm.hpp>

#include "UgcModel.h"

/**
 * Rays against a mesh's triangles: the nearest hit (never the triangle a ray leaves) and, for the ambient occlusion
 * rays, whether anything is hit, by one of several backends (the ray_backend setting, or per job):
 *   embree:  Intel's Embree 4 on the CPU, on the thread that asks (no threads of its own); always there
 *   hiprt:   AMD's HIPRT on the GPU (AMD through HIP, NVIDIA through CUDA, loaded when first asked for by Orochi),
 *            when built with DLU_HIPRT and a GPU is there; else embree. One GPU for the process, used by one thread
 *            at a time; its time is not CPU time.
 *   embree-gpu: Embree 4 on an Intel GPU (Arc, Xe) through SYCL, when built with DLU_EMBREE_SYCL and such a GPU is
 *            there; else embree. The same way: one GPU for the process, a thread at a time.
 * A scene is built and traced on the thread that asks, so its time counts towards that thread's CPU time
 * (UgcThrottle). Scenes aren't shared between threads. docs/UgcServer.md ("Processing options") has the details.
 */
namespace UgcRays {
	constexpr uint32_t NONE = std::numeric_limits<uint32_t>::max();
	constexpr float INF = std::numeric_limits<float>::infinity();

	enum class eBackend : uint8_t { EMBREE = 0, HIPRT, EMBREE_GPU };

	// The setting's name of a backend (embree, hiprt, embree-gpu)
	std::string_view Name(eBackend backend);
	// A backend by its name (case sensitive; builtin, the backend Embree replaced, is embree); nullopt for anything else
	std::optional<eBackend> Parse(std::string_view name);
	// Whether this build and machine can use the backend (embree always)
	bool Available(eBackend backend);
	// The backend that is used when `wanted` is asked for: itself, or embree when it isn't available
	eBackend Resolve(eBackend wanted);
	// Why a backend isn't available (empty when it is)
	std::string Problem(eBackend backend);

	struct Hit {
		float t{ INF };
		uint32_t triangle{ NONE };
		float u{}, v{}; // weights of the triangle's second and third vertex
	};

	// A ray of a batch (the layout the GPU kernels read too)
	struct Ray {
		glm::vec3 origin{};
		float minT{};            // Occluded: hits further than this count (Closest: further than 0)
		glm::vec3 direction{};   // unit
		float maxT{ INF };       // hits nearer than this count
		uint32_t skip{ NONE };   // Closest: the triangle never hit (the one the ray leaves)
		uint32_t padding[3]{};
	};
	static_assert(sizeof(Ray) == 48, "the GPU kernels read rays as 48 bytes");
	static_assert(sizeof(Hit) == 16, "the GPU kernels write hits as 16 bytes");

	class Scene {
	public:
		virtual ~Scene() = default;

		// The nearest triangle along the ray (unit direction) further than 0 and before `maxT`, never `skip` (the
		// triangle the ray leaves, as in Cycles); triangle NONE (and t = maxT) when there is none
		virtual Hit Closest(const glm::vec3& origin, const glm::vec3& direction, uint32_t skip = NONE, float maxT = INF) const = 0;

		// Whether the ray (unit direction) hits a triangle further than `minT` and nearer than `maxT`
		virtual bool Occluded(const glm::vec3& origin, const glm::vec3& direction, float minT, float maxT) const = 0;

		// Many rays at once, as the single ray queries answer them (a GPU answers a batch at the cost of one ray)
		virtual void Closest(const Ray* rays, Hit* hits, size_t count) const;
		virtual void Occluded(const Ray* rays, uint8_t* occluded, size_t count) const;

		// Whether the backend is only fast with big batches (a GPU): the callers then trace many paths side by side
		virtual bool PrefersBatches() const { return false; }
	};

	/**
	 * The mesh's triangles as they are now (copied) in the backend Resolve(backend) picks. Throws std::runtime_error
	 * when the backend fails.
	 */
	std::unique_ptr<Scene> Make(eBackend backend, const UgcModel::Mesh& mesh);

	// Which GPU a GPU backend uses (hiprt_device: 0 is the first HIP or CUDA device; embree_gpu_device: 0 is the first
	// Intel GPU Embree supports); before it is first used
	void SetGpuDevice(eBackend backend, int index);
}
