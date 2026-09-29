#pragma once

#include <memory>
#include <string>

#include "UgcRays.h"

/**
 * UgcRays' embree-gpu backend (built with DLU_EMBREE_SYCL): Embree 4 on an Intel GPU (Arc, Xe) through SYCL, in
 * libdlu_embree_sycl next to the servers (built by a SYCL compiler, dUgcServer/EmbreeSycl), loaded when first asked for.
 * One GPU for the process; the threads take turns on it.
 */
namespace UgcRaysEmbreeGpu {
	// Whether it can be used: loads the library and sets the GPU up the first time
	bool Available();

	// Why it can't (empty when it can, or before it was first asked)
	std::string Problem();

	// The mesh on the GPU; null when the GPU fails (the caller uses another backend)
	std::unique_ptr<UgcRays::Scene> Make(const UgcModel::Mesh& mesh);

	// Which of the GPUs Embree supports (0: the first); before it is first used
	void SetDevice(int index);
}
