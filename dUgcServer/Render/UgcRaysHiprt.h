#pragma once

#include <memory>
#include <string>

#include "UgcRays.h"

/**
 * UgcRays' hiprt backend (built with DLU_HIPRT): AMD's HIPRT on the GPU, with HIP or CUDA loaded at run time through
 * Orochi. One GPU context for the process; the threads take turns on it. The trace kernels are compiled the first time
 * (HIPRT keeps them in its cache folder after that).
 */
namespace UgcRaysHiprt {
	// Whether the GPU can be used: loads HIP or CUDA and HIPRT, makes the context and compiles the kernels the first time
	bool Available();

	// Why it can't (empty when it can, or before it was first asked)
	std::string Problem();

	// The mesh on the GPU; null when the GPU fails (the caller uses another backend)
	std::unique_ptr<UgcRays::Scene> Make(const UgcModel::Mesh& mesh);

	// Which device (0: the first); before it is first used
	void SetDevice(int index);
}
