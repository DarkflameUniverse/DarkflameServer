#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "UgcModel.h"
#include "UgcRays.h"

/**
 * Hidden surface removal as LU Toolbox's Remove Hidden Faces decides it, without its texture: paths are traced from
 * points on each opaque triangle under a sky of overwhelming brightness, bouncing off the model; a triangle none of
 * whose paths reaches the sky is removed. So faces seen only through openings, or lit only by light bounced in
 * (interiors, recesses), stay; faces sealed inside go. docs/UgcServer.md ("Hidden faces") has the details.
 */
namespace UgcHsr {
	struct Options {
		bool enabled{ true };           // remove_hidden_faces
		bool groundPlane{ false };      // hsr_ground_plane: LU Toolbox's black box under the model (y 0 down to -100)
		int samples{ 8 };               // hsr_samples: paths traced from each point (LU Toolbox's Samples)
		int bounces{ 8 };               // hsr_bounces: bounces a path may take (the Cycles bake's Max Bounces)
		float spacing{ 0.1143f };       // hsr_sample_spacing: LDD units between points (a stud is 0.8: 7 points along it)
		int minPoints{ 28 };            // hsr_min_points: points on a triangle at least (LU Toolbox bakes 28 texels a triangle)
		uint64_t seed{};                // of the paths' random numbers (the same seed gives the same result)
		UgcRays::eBackend rays{};       // what traces the paths' rays (ugc_ray_backend)
	};

	struct Result {
		size_t trianglesBefore{};  // opaque and transparent
		size_t trianglesRemoved{};
		std::vector<bool> kept;    // per opaque triangle before: whether it stayed
		uint64_t points{};         // sample points traced from
		uint64_t paths{};          // paths traced
	};

	/**
	 * Which triangles of `mesh` a path from them reaches the sky from (triangles without area: none). `mesh` is
	 * everything that occludes (the opaque bricks; transparent ones hide nothing, as in LU Toolbox).
	 */
	std::vector<bool> Visible(const UgcModel::Mesh& mesh, const Options& options, uint64_t* points = nullptr, uint64_t* paths = nullptr);

	// Removes the opaque triangles that aren't Visible (nothing when options.enabled is off); transparent ones stay
	Result RemoveHiddenFaces(UgcModel::Model& model, const Options& options);

	/**
	 * The points on triangle a, b, c paths start from, as barycentric weights (of a, b, c): rows parallel to its
	 * longest side, `spacing` apart, with points `spacing` apart along each row (so about area / spacing^2 of them).
	 * When that gives fewer than `minimum` the rows and points are closer together until there are that many; at
	 * least 4 (its centre and one towards each corner), at most 4096 (big triangles get them further apart).
	 */
	std::vector<glm::vec3> SamplePoints(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, float spacing, size_t minimum = 4);
}
