#pragma once

#include <cstddef>
#include <vector>

#include "UgcModel.h"

/**
 * Hidden surface removal: the opaque mesh rendered from 42 directions around the model (UgcRender::VisibleFromAround),
 * and the triangles that show in none of them removed. Transparent bricks hide nothing and aren't touched, as in LU
 * Toolbox. Faces seen only by bounced light (insides seen through openings, recesses) don't show in the renders and
 * are removed too. docs/UgcServer.md ("Hidden faces") has the details.
 */
namespace UgcHsr {
	struct Options {
		bool enabled{ true };       // remove_hidden_faces
		bool groundPlane{ false };  // hsr_ground_plane: nothing is seen from below the model (LU Toolbox's Use Ground Plane)
		int resolution{ 1024 };     // hsr_resolution: pixels square of each render
	};

	struct Result {
		size_t trianglesBefore{};  // opaque and transparent
		size_t trianglesRemoved{};
		std::vector<bool> kept;    // per opaque triangle before: whether it stayed
	};

	// Removes the opaque triangles that show in none of the renders (nothing when options.enabled is off)
	Result RemoveHiddenFaces(UgcModel::Model& model, const Options& options);
}
