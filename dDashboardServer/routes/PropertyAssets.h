#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "json.hpp"
#include "dCommonVars.h"
#include "IPropertyContents.h"

/**
 * What the 3D property viewer loads for a placed model, shared by the property pages (APIRoutes.cpp) and the
 * showcase (Showcase.cpp). Callers decide who may see the property; these only read the data.
 */
namespace PropertyAssets {
	// A placed model's LXFML and a file name for it: player-built models from the ugc table, prebuilt ones from the client
	std::optional<std::pair<std::string, std::string>> ModelLxfml(const IPropertyContents::Model& model, LWOOBJID propertyId);

	// A placed model's behaviors, parsed for the viewer
	nlohmann::json ModelBehaviors(const IPropertyContents::Model& model);

	// Every LDD geometry file of a brick design in one bundle (format in APIRoutes.cpp); nullopt if there are none
	std::optional<std::string> BrickBundle(uint32_t lod, uint32_t design);
}
