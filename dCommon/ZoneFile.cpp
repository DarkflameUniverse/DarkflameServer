#include "ZoneFile.h"

#include <algorithm>
#include <cctype>
#include <istream>
#include <stdexcept>
#include <string>

#include "BinaryIO.h"
#include "Game.h"
#include "Logger.h"

void ZoneFile::ReadHeader(std::istream& file) {
	BinaryIO::BinaryRead(file, fileFormatVersion);
	// Before PrePreAlpha a scene is only its ID, with no file to load it from (LuzReader::ReadScenes)
	if (fileFormatVersion < FileFormatVersion::PrePreAlpha) {
		throw std::runtime_error("Zone file version " + std::to_string(static_cast<uint32_t>(fileFormatVersion)) + " is older than " + std::to_string(static_cast<uint32_t>(FileFormatVersion::PrePreAlpha)) + ": its scenes have no files");
	}

	if (fileFormatVersion >= FileFormatVersion::Alpha) BinaryIO::BinaryRead(file, mapRevision);

	BinaryIO::BinaryRead(file, worldID);

	if (fileFormatVersion >= FileFormatVersion::Beta) {
		BinaryIO::BinaryRead(file, spawnpoint);
		BinaryIO::BinaryRead(file, spawnpointRotation);
	}

	// A u8 before LateAlpha, a u32 from it on (LuzFile::ReadLUZFile)
	uint32_t sceneCount = 0;
	if (fileFormatVersion < FileFormatVersion::LateAlpha) {
		uint8_t count;
		BinaryIO::BinaryRead(file, count);
		sceneCount = count;
	} else BinaryIO::BinaryRead(file, sceneCount);

	for (uint32_t i = 0; i < sceneCount; ++i) {
		ReadScene(file);
	}

	ReadZoneBoundaries(file);

	//Read generic zone info:
	BinaryIO::ReadString<uint8_t>(file, zoneRawPath, BinaryIO::ReadType::String);
	// PrePreAlpha files have no name or description (LuzFile::ReadLUZFile)
	if (fileFormatVersion > FileFormatVersion::PrePreAlpha) {
		BinaryIO::ReadString<uint8_t>(file, zoneName, BinaryIO::ReadType::String);
		BinaryIO::ReadString<uint8_t>(file, zoneDesc, BinaryIO::ReadType::String);
	}
}

void ZoneFile::Read(std::istream& file) {
	ReadHeader(file);

	if (fileFormatVersion >= FileFormatVersion::PreAlpha) {
		uint32_t transitionCount = 0;
		BinaryIO::BinaryRead(file, transitionCount);
		for (uint32_t i = 0; i < transitionCount; ++i) {
			ReadSceneTransition(file);
		}
	}

	if (fileFormatVersion >= FileFormatVersion::EarlyAlpha) {
		BinaryIO::BinaryRead(file, pathDataLength);
		BinaryIO::BinaryRead(file, pathChunkVersion); // always should be 1

		uint32_t pathCount;
		BinaryIO::BinaryRead(file, pathCount);

		paths.reserve(pathCount);
		for (uint32_t i = 0; i < pathCount; ++i) ReadPath(file);
	}
}

void ZoneFile::ReadScene(std::istream& file) {
	ZoneScene scene;

	BinaryIO::ReadString<uint8_t>(file, scene.filename, BinaryIO::ReadType::String);

	if (fileFormatVersion >= FileFormatVersion::LatePreAlpha) {
		BinaryIO::BinaryRead(file, scene.id);
		BinaryIO::BinaryRead(file, scene.sceneType);

		BinaryIO::ReadString<uint8_t>(file, scene.name, BinaryIO::ReadType::String);
	}

	if (fileFormatVersion == FileFormatVersion::LatePreAlpha) {
		BinaryIO::BinaryRead(file, scene.unknown1);
		BinaryIO::BinaryRead(file, scene.unknown2);
	}

	if (fileFormatVersion >= FileFormatVersion::LatePreAlpha) {
		BinaryIO::BinaryRead(file, scene.color_r);
		BinaryIO::BinaryRead(file, scene.color_b);
		BinaryIO::BinaryRead(file, scene.color_g);
	}

	scenes.push_back(std::move(scene));
}

void ZoneFile::ReadZoneBoundaries(std::istream& file) {
	uint8_t count = 0;
	BinaryIO::BinaryRead(file, count);
	zoneBoundaries.reserve(count);
	for (uint8_t i = 0; i < count; ++i) {
		ZoneBoundary boundary;
		BinaryIO::BinaryRead(file, boundary.normal);
		BinaryIO::BinaryRead(file, boundary.point);
		// The client reads one u32 of map ID (low 16 bits) and instance ID (high 16 bits)
		LWOMAPID mapID = 0;
		LWOINSTANCEID instanceID = 0;
		BinaryIO::BinaryRead(file, mapID);
		BinaryIO::BinaryRead(file, instanceID);
		boundary.destZoneID = LWOZONEID(mapID, instanceID, 0);
		BinaryIO::BinaryRead(file, boundary.destSceneID);
		BinaryIO::BinaryRead(file, boundary.spawnLocation);
		zoneBoundaries.push_back(boundary);
	}
}

void ZoneFile::ReadSceneTransition(std::istream& file) {
	SceneTransition sceneTrans;
	if (fileFormatVersion < FileFormatVersion::Auramar) {
		BinaryIO::ReadString<uint8_t>(file, sceneTrans.name, BinaryIO::ReadType::String);
		BinaryIO::BinaryRead(file, sceneTrans.width);
	}

	//BROTHER MAY I HAVE SOME LOOPS?
	uint8_t loops = (fileFormatVersion <= FileFormatVersion::LatePreAlpha || fileFormatVersion >= FileFormatVersion::Launch) ? 2 : 5;

	sceneTrans.points.reserve(loops);
	for (uint8_t i = 0; i < loops; ++i) {
		sceneTrans.points.push_back(ReadSceneTransitionInfo(file));
	}

	sceneTransitions.push_back(sceneTrans);
}

SceneTransitionInfo ZoneFile::ReadSceneTransitionInfo(std::istream& file) {
	SceneTransitionInfo info;
	BinaryIO::BinaryRead(file, info.sceneID);
	BinaryIO::BinaryRead(file, info.position);
	return info;
}

void ZoneFile::ReadLdfConfig(std::istream& file, PathType pathType, PathWaypoint& waypoint) {
	uint32_t count;
	BinaryIO::BinaryRead(file, count);
	for (uint32_t i = 0; i < count; ++i) {
		std::string parameter;
		BinaryIO::ReadString<uint8_t>(file, parameter, BinaryIO::ReadType::WideString);

		std::string value;
		BinaryIO::ReadString<uint8_t>(file, value, BinaryIO::ReadType::WideString);

		if (pathType == PathType::Movement || pathType == PathType::Rail) {
			// cause NetDevil puts spaces in things that don't need spaces
			parameter.erase(std::remove_if(parameter.begin(), parameter.end(), ::isspace), parameter.end());
			auto waypointCommand = WaypointCommandType::StringToWaypointCommandType(parameter);
			if (waypointCommand == eWaypointCommandType::DELAY) value.erase(std::remove_if(value.begin(), value.end(), ::isspace), value.end());
			if (waypointCommand != eWaypointCommandType::INVALID) {
				auto& command = waypoint.commands.emplace_back();
				command.command = waypointCommand;
				command.data = value;
			} else LOG("Tried to load invalid waypoint command '%s'", parameter.c_str());
		} else {
			waypoint.config.ParseInsert(parameter + "=" + value);
		}
	}
}

void ZoneFile::ReadPath(std::istream& file) {
	Path path = Path();

	BinaryIO::BinaryRead(file, path.pathVersion);

	BinaryIO::ReadString<uint8_t>(file, path.pathName, BinaryIO::ReadType::WideString);

	if (path.pathVersion < 3) {
		// Before version 3 the type is a name, "platform" or "npc" (LevelPath::FromBuffer)
		std::string typeName;
		BinaryIO::ReadString<uint8_t>(file, typeName, BinaryIO::ReadType::WideString);
		path.pathType = typeName == "platform" ? PathType::MovingPlatform : PathType::Movement;
	} else BinaryIO::BinaryRead(file, path.pathType);
	BinaryIO::BinaryRead(file, path.flags);
	BinaryIO::BinaryRead(file, path.pathBehavior);

	if (path.pathType == PathType::MovingPlatform) {
		if (path.pathVersion >= 18) {
			BinaryIO::BinaryRead(file, path.movingPlatform.timeBasedMovement);
		} else if (path.pathVersion >= 13) {
			BinaryIO::ReadString<uint8_t>(file, path.movingPlatform.platformTravelSound, BinaryIO::ReadType::WideString);
		}
	} else if (path.pathType == PathType::Property) {
		BinaryIO::BinaryRead(file, path.property.pathType);
		BinaryIO::BinaryRead(file, path.property.price);
		BinaryIO::BinaryRead(file, path.property.rentalTime);
		BinaryIO::BinaryRead(file, path.property.associatedZone);

		if (path.pathVersion >= 5) {
			BinaryIO::ReadString<uint8_t>(file, path.property.displayName, BinaryIO::ReadType::WideString);
			BinaryIO::ReadString<uint32_t>(file, path.property.displayDesc, BinaryIO::ReadType::WideString);
		}

		if (path.pathVersion >= 6) BinaryIO::BinaryRead(file, path.property.type);

		if (path.pathVersion >= 7) {
			BinaryIO::BinaryRead(file, path.property.cloneLimit);
			BinaryIO::BinaryRead(file, path.property.repMultiplier);
			BinaryIO::BinaryRead(file, path.property.rentalPeriod);
		}

		if (path.pathVersion >= 8) {
			BinaryIO::BinaryRead(file, path.property.achievementRequired);
			BinaryIO::BinaryRead(file, path.property.playerZoneCoords);
			BinaryIO::BinaryRead(file, path.property.maxBuildHeight);
		}
	} else if (path.pathType == PathType::Camera) {
		BinaryIO::ReadString<uint8_t>(file, path.camera.nextPath, BinaryIO::ReadType::WideString);
		if (path.pathVersion >= 14) {
			BinaryIO::BinaryRead(file, path.camera.rotatePlayer);

		}
	} else if (path.pathType == PathType::Spawner) {
		BinaryIO::BinaryRead(file, path.spawner.spawnedLOT);
		BinaryIO::BinaryRead(file, path.spawner.respawnTime);
		BinaryIO::BinaryRead(file, path.spawner.maxToSpawn);
		BinaryIO::BinaryRead(file, path.spawner.amountMaintained);
		BinaryIO::BinaryRead(file, path.spawner.spawnerObjID);
		BinaryIO::BinaryRead(file, path.spawner.spawnerNetActive);
	}

	// Read waypoints

	BinaryIO::BinaryRead(file, path.waypointCount);
	path.pathWaypoints.reserve(path.waypointCount);
	for (uint32_t i = 0; i < path.waypointCount; ++i) {
		PathWaypoint waypoint = PathWaypoint();

		BinaryIO::BinaryRead(file, waypoint.position.x);
		BinaryIO::BinaryRead(file, waypoint.position.y);
		BinaryIO::BinaryRead(file, waypoint.position.z);

		if (path.pathVersion < 3) {
			// Before version 3 every waypoint has a moving platform's data and then name/value pairs, whatever its path's type
			BinaryIO::BinaryRead(file, waypoint.rotation.w);
			BinaryIO::BinaryRead(file, waypoint.rotation.x);
			BinaryIO::BinaryRead(file, waypoint.rotation.y);
			BinaryIO::BinaryRead(file, waypoint.rotation.z);
			BinaryIO::BinaryRead(file, waypoint.movingPlatform.lockPlayer);
			BinaryIO::BinaryRead(file, waypoint.speed);
			BinaryIO::BinaryRead(file, waypoint.movingPlatform.wait);
			ReadLdfConfig(file, path.pathType, waypoint);
			path.pathWaypoints.push_back(waypoint);
			continue;
		}

		if (path.pathType == PathType::Spawner || path.pathType == PathType::MovingPlatform || path.pathType == PathType::Race || path.pathType == PathType::Camera || path.pathType == PathType::Rail) {
			BinaryIO::BinaryRead(file, waypoint.rotation.w);
			BinaryIO::BinaryRead(file, waypoint.rotation.x);
			BinaryIO::BinaryRead(file, waypoint.rotation.y);
			BinaryIO::BinaryRead(file, waypoint.rotation.z);
		}

		if (path.pathType == PathType::MovingPlatform) {
			BinaryIO::BinaryRead(file, waypoint.movingPlatform.lockPlayer);
			BinaryIO::BinaryRead(file, waypoint.speed);
			BinaryIO::BinaryRead(file, waypoint.movingPlatform.wait);
			if (path.pathVersion >= 13) {
				BinaryIO::ReadString<uint8_t>(file, waypoint.movingPlatform.departSound, BinaryIO::ReadType::WideString);
				BinaryIO::ReadString<uint8_t>(file, waypoint.movingPlatform.arriveSound, BinaryIO::ReadType::WideString);
			}
		} else if (path.pathType == PathType::Camera) {
			BinaryIO::BinaryRead(file, waypoint.camera.time);
			BinaryIO::BinaryRead(file, waypoint.camera.fov);
			BinaryIO::BinaryRead(file, waypoint.camera.tension);
			BinaryIO::BinaryRead(file, waypoint.camera.continuity);
			BinaryIO::BinaryRead(file, waypoint.camera.bias);
		} else if (path.pathType == PathType::Race) {
			BinaryIO::BinaryRead(file, waypoint.racing.isResetNode);
			BinaryIO::BinaryRead(file, waypoint.racing.isNonHorizontalCamera);
			BinaryIO::BinaryRead(file, waypoint.racing.planeWidth);
			BinaryIO::BinaryRead(file, waypoint.racing.planeHeight);
			BinaryIO::BinaryRead(file, waypoint.racing.shortestDistanceToEnd);
		} else if (path.pathType == PathType::Rail) {
			if (path.pathVersion > 16) BinaryIO::BinaryRead(file, waypoint.speed);
		}

		// object LDF configs
		if (path.pathType == PathType::Movement || path.pathType == PathType::Spawner || path.pathType == PathType::Rail) {
			ReadLdfConfig(file, path.pathType, waypoint);
		}

		path.pathWaypoints.push_back(waypoint);
	}
	paths.push_back(path);
}
