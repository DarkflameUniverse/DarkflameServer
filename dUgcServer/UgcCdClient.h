#pragma once

#include <string>

#include "UgcJobs.h"

// What the UGC server reads from the CDClient (main thread only: the CDClient connection isn't shared with workers)
namespace UgcCdClient {
	// A modular build's modules (ugc_modular_build.ldf_config) with their render assets and module data, and the
	// ModularBuildComponent of their build type; false and `error` when it can't be put together
	bool GatherModular(const std::string& modules, UgcJobs::ModularInput& input, std::string& error);
}
