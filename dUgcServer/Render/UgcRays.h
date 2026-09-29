#pragma once

#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string_view>

#include <glm/glm.hpp>

#include "UgcModel.h"

/**
 * Rays against a mesh's triangles, for the hidden faces' paths (the nearest hit) and the ambient occlusion rays
 * (whether anything is hit), by one of several backends (the ugc_ray_backend setting, or per job):
 *   builtin: the UGC server's own bounding volume hierarchies (the ones it always had)
 *   embree:  Intel's Embree 4 on the CPU, on the thread that asks (no threads of its own)
 * A scene is built and traced on the thread that asks, so its time counts towards that thread's CPU time
 * (UgcThrottle). Scenes aren't shared between threads. docs/UgcServer.md ("Processing options") has the details.
 */
namespace UgcRays {
	constexpr uint32_t NONE = std::numeric_limits<uint32_t>::max();
	constexpr float INF = std::numeric_limits<float>::infinity();

	enum class eBackend : uint8_t { BUILTIN = 0, EMBREE, HIPRT };

	// The setting's name of a backend (builtin, embree, hiprt)
	std::string_view Name(eBackend backend);
	// A backend by its name (case sensitive); nullopt for anything else
	std::optional<eBackend> Parse(std::string_view name);
	// Whether this build and machine can use the backend (builtin and embree always)
	bool Available(eBackend backend);
	// The backend that is used when `wanted` is asked for: itself, or embree when it isn't available
	eBackend Resolve(eBackend wanted);

	struct Hit {
		float t{ INF };
		uint32_t triangle{ NONE };
		float u{}, v{}; // weights of the triangle's second and third vertex
	};

	class Scene {
	public:
		virtual ~Scene() = default;

		// The nearest triangle along the ray (unit direction) further than 0 and before `maxT`, never `skip` (the
		// triangle the ray leaves, as in Cycles); triangle NONE (and t = maxT) when there is none
		virtual Hit Closest(const glm::vec3& origin, const glm::vec3& direction, uint32_t skip = NONE, float maxT = INF) const = 0;

		// Whether the ray (unit direction) hits a triangle further than `minT` and nearer than `maxT`
		virtual bool Occluded(const glm::vec3& origin, const glm::vec3& direction, float minT, float maxT) const = 0;
	};

	/**
	 * The mesh's triangles as they are now (copied) in the backend Resolve(backend) picks. Throws std::runtime_error
	 * when the backend fails.
	 */
	std::unique_ptr<Scene> Make(eBackend backend, const UgcModel::Mesh& mesh);
}
