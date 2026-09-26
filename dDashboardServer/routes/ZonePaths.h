#pragma once

#include <cstdint>
#include <exception>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "ZoneFile.h"

/**
 * Property build areas and scene files from a zone file (.luz), for the 3D views and the rescue page.
 *
 * A property zone has a path of type Property: its waypoints outline where players may build, and it carries the
 * highest point they may build to. The file is read by ZoneFile (dCommon), the same reader the world server's
 * Zone::LoadZoneIntoMemory uses. Pure (bytes in, areas out) so it can be unit tested.
 */
namespace ZonePaths {
	struct Point {
		float x{};
		float y{};
		float z{};
	};

	struct PropertyArea {
		std::string name;        // the path's name, e.g. "PropertyPath"
		std::string displayName; // what the property is called when rented
		int32_t areaType{};      // 0: the waypoints outline it, 1: the entire zone, 2: a generated rectangle
		float maxBuildHeight{};  // 0 when the file doesn't say
		std::vector<Point> outline;
	};

	/**
	 * The zone file, or nullopt and error if it ends early. `headerOnly` stops after the scenes and the terrain file's
	 * name (ZoneFile::ReadHeader), skipping the scene transitions and paths, which is all most views need.
	 */
	inline std::optional<ZoneFile> Read(const std::string& luz, std::string& error, bool headerOnly = false) {
		std::istringstream stream(luz);
		ZoneFile zone;
		try {
			if (headerOnly) zone.ReadHeader(stream);
			else zone.Read(stream);
		} catch (const std::exception&) {
			stream.setstate(std::ios::failbit);
		}
		if (stream.fail()) {
			error = "The zone file is shorter than expected";
			return std::nullopt;
		}
		return zone;
	}

	// The start of the zone file: version, spawn point, scenes and the terrain file's name
	inline std::optional<ZoneFile> ReadHeader(const std::string& luz, std::string& error) {
		return Read(luz, error, true);
	}

	// The zone's scene files (.lvl), relative to the folder the .luz is in; empty if the file can't be read
	inline std::vector<std::string> ReadSceneFiles(const std::string& luz) {
		std::string error;
		const auto zone = ReadHeader(luz, error);
		std::vector<std::string> files;
		if (zone) for (const auto& scene : zone->scenes) files.push_back(scene.filename);
		return files;
	}

	inline std::optional<std::vector<PropertyArea>> ReadPropertyAreas(const std::string& luz, std::string& error) {
		const auto zone = Read(luz, error);
		if (!zone) return std::nullopt;
		std::vector<PropertyArea> areas;
		for (const auto& path : zone->paths) {
			if (path.pathType != PathType::Property) continue;
			PropertyArea area{ path.pathName, path.property.displayName, static_cast<int32_t>(path.property.pathType), path.property.maxBuildHeight };
			for (const auto& waypoint : path.pathWaypoints) area.outline.push_back({ waypoint.position.x, waypoint.position.y, waypoint.position.z });
			areas.push_back(std::move(area));
		}
		return areas;
	}
}
