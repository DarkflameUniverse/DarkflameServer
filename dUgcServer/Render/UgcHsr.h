#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>

#include "UgcModel.h"
#include "UgcRays.h"

/**
 * Hidden surface removal as LU Toolbox's Remove Hidden Faces decides it, without its texture: paths are traced from
 * points on each opaque triangle under a sky of overwhelming brightness, bouncing off the model; a triangle none of
 * whose paths reaches the sky is removed. So faces seen only through openings, or lit only by light bounced in
 * (interiors, recesses), stay; faces sealed inside go. docs/UgcServer.md ("Hidden faces") has the details.
 * The fast method (hsr_method=fast) is the test the UGC server used before: renders from 42 directions around the
 * model (UgcRender::VisibleFromAround), which removes faces seen only by bounced light too.
 */
namespace UgcHsr {
	// How hidden faces are found (hsr_method): LU Toolbox's paths, or the renders from around the model
	enum class eMethod : uint8_t { TOOLBOX = 0, FAST };

	// The setting's name of a method (toolbox, fast)
	std::string_view Name(eMethod method);
	// A method by its name; nullopt for anything else
	std::optional<eMethod> Parse(std::string_view name);

	struct Options {
		bool enabled{ true };           // remove_hidden_faces
		eMethod method{};               // hsr_method
		bool groundPlane{ false };      // hsr_ground_plane: LU Toolbox's black box under the model (y 0 down to -100)
		int samples{ 8 };               // hsr_samples: paths traced from each point (LU Toolbox's Samples)
		int bounces{ 8 };               // hsr_bounces: bounces a path may take (the Cycles bake's Max Bounces)
		float spacing{ 0.1143f };       // hsr_sample_spacing: LDD units between points (a stud is 0.8: 7 points along it)
		int minPoints{ 28 };            // hsr_min_points: points on a triangle at least (LU Toolbox bakes 28 texels a triangle)
		uint64_t seed{};                // of the paths' random numbers (the same seed gives the same result)
		UgcRays::eBackend rays{};       // what traces the paths' rays (ray_backend)
		int fastResolution{ 1024 };     // hsr_fast_resolution: pixels square of each of the fast method's renders
	};

	struct Result {
		size_t trianglesBefore{};  // opaque and transparent
		size_t trianglesRemoved{};
		std::vector<bool> kept;    // per opaque triangle before: whether it stayed
		uint64_t points{};         // sample points traced from (toolbox)
		uint64_t paths{};          // paths traced (toolbox)
	};

	/**
	 * Which triangles of `mesh` a path from them reaches the sky from (triangles without area: none). `mesh` is
	 * everything that occludes (the opaque bricks; transparent ones hide nothing, as in LU Toolbox).
	 */
	std::vector<bool> Visible(const UgcModel::Mesh& mesh, const Options& options, uint64_t* points = nullptr, uint64_t* paths = nullptr);

	// Removes the opaque triangles that aren't Visible, or with the fast method don't show in its renders (nothing when
	// options.enabled is off); transparent ones stay
	Result RemoveHiddenFaces(UgcModel::Model& model, const Options& options);

	/**
	 * The points on triangle a, b, c paths start from, as barycentric weights (of a, b, c): rows parallel to its
	 * longest side, `spacing` apart, with points `spacing` apart along each row (so about area / spacing^2 of them).
	 * When that gives fewer than `minimum` the rows and points are closer together until there are that many; at
	 * least 4 (its centre and one towards each corner), at most 4096 (big triangles get them further apart).
	 */
	std::vector<glm::vec3> SamplePoints(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, float spacing, size_t minimum = 4);
}
