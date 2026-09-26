#pragma once

#include <cstdint>
#include <exception>
#include <map>
#include <sstream>
#include <string>

#include "GeneralUtils.h"
#include "LevelFile.h"

/**
 * Which features objects in a scene file (.lvl) are gated on, for the events calendar.
 *
 * A world server skips an object whose `gatingOnFeature` names a feature that is neither one of the event_1..event_8
 * settings nor unlocked for the configured client version in the FeatureGating table (Level::LoadSceneObjects in
 * dZoneManager/Level.cpp). Objects are only read when a zone loads, so switching an event on or off changes what a
 * zone has only for world instances started afterwards. The file is read by LevelFile (dCommon), as the world reads
 * it; client-only objects are left out, as the world skips them. Pure (bytes in, counts out) so it can be unit tested.
 */
namespace LevelGating {
	// Feature name -> how many objects in the scene are gated on it, as far as the file could be read
	inline std::map<std::string, uint32_t> ReadGatedFeatures(const std::string& lvl) {
		std::istringstream stream(lvl);
		LevelFile level;
		try {
			level.Read(stream);
		} catch (const std::exception&) {
			// A damaged file: keep the objects read before the damage
		}
		std::map<std::string, uint32_t> features;
		for (const auto& object : level.objects) {
			const auto setting = [&object](const std::u16string& key) {
				const auto it = object.settings.find(key);
				return it == object.settings.end() || !it->second ? std::string{} : it->second->GetValueAsString();
			};
			auto feature = setting(u"gatingOnFeature");
			while (!feature.empty() && (feature.back() == '\r' || feature.back() == ' ')) feature.pop_back();
			if (feature.empty() || GeneralUtils::TryParse(setting(u"loadOnClientOnly"), false)) continue;
			features[feature]++;
		}
		return features;
	}
}
