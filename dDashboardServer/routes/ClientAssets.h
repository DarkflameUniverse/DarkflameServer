#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

#include "dCommonVars.h"
#include "json.hpp"

/**
 * Game client data for the dashboard: item details for tooltips, icons, and a read-only browser over the
 * client's res folder (client_location). Textures are converted to PNG with ImageMagick and cached.
 */
namespace ClientAssets {
	std::string ItemName(LOT lot);

	// An object's raw Objects.name (not localized), or nullopt when the LOT isn't in the CDClient
	std::optional<std::string> ObjectName(LOT lot);

	// Name, description, rarity, stat bonuses, skills and item set (with set bonuses), cached per LOT
	nlohmann::json ItemInfo(LOT lot);

	std::string IconPathForLot(LOT lot);
	std::string IconPathForId(int iconId);

	// Relative path inside res/ using only safe characters and no traversal
	bool IsSafeAssetPath(const std::string& path);

	// A DDS or PNG texture from res/ as PNG, scaled down to fit maxSize. `opaque` drops the alpha channel first, for
	// textures whose alpha isn't transparency (terrain textures have alpha 0 everywhere; kept, they'd come out black)
	std::optional<std::string> TextureAsPng(const std::string& assetPath, uint32_t maxSize, bool opaque = false);

	// Raw bytes of a file inside res/ (matched ignoring case)
	std::optional<std::string> ReadResFile(const std::string& relativePath);

	// Where a file inside res/ is on disk (matched ignoring case), if it exists
	std::optional<std::filesystem::path> ResolveResFile(const std::string& relativePath);

	// The res/-relative path of a file called `fileName` (ignoring case) anywhere under res/<folder>
	std::optional<std::string> FindResFile(const std::string& folder, const std::string& fileName);

	// Entries of a folder inside res/
	std::optional<nlohmann::json> ListDirectory(const std::string& relativePath);
}

void RegisterClientAssetRoutes();
