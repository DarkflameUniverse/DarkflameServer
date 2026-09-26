#pragma once

#include <cstdint>
#include <exception>
#include <sstream>
#include <string>
#include <vector>

#include "GeneralUtils.h"
#include "LevelFile.h"

/**
 * Spawn points from a scene file (.lvl), so staff can pick where in a zone a rescued player lands.
 *
 * A spawn point is any object whose settings carry `respawnname`: the world server registers those by that name
 * (EntityManager::ConstructEntity) and puts an arriving player on the one their character's target scene names
 * (Entity::Initialize), the way rocket launchers and /gmzone pick a landing spot. The file is read by LevelFile
 * (dCommon), the same reader the world's Level uses; objects the world skips (loadOnClientOnly) are skipped here too.
 * Pure (bytes in, points out) so it can be unit tested.
 */
namespace LevelObjects {
	struct SpawnPoint {
		std::string name; // the respawnname, e.g. "NS_LW_Portal"
		uint32_t lot{};
		float x{};
		float y{};
		float z{};
	};

	// The spawn points, as far as the file could be read
	inline std::vector<SpawnPoint> ReadSpawnPoints(const std::string& lvl) {
		std::istringstream stream(lvl);
		LevelFile level;
		try {
			level.Read(stream);
		} catch (const std::exception&) {
			// A damaged file: keep the objects read before the damage
		}
		std::vector<SpawnPoint> points;
		for (const auto& object : level.objects) {
			const auto setting = [&object](const std::u16string& key) {
				const auto it = object.settings.find(key);
				return it == object.settings.end() || !it->second ? std::string{} : it->second->GetValueAsString();
			};
			auto name = setting(u"respawnname");
			while (!name.empty() && (name.back() == '\r' || name.back() == ' ')) name.pop_back();
			// Level::LoadSceneObjects skips these
			if (name.empty() || GeneralUtils::TryParse(setting(u"loadOnClientOnly"), false)) continue;
			points.push_back({ std::move(name), static_cast<uint32_t>(object.lot), object.position.x, object.position.y, object.position.z });
		}
		return points;
	}
}
