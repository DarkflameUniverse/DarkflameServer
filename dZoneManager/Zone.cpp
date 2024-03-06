#include "Zone.h"
#include "Level.h"
#include <fstream>
#include <sstream>
#include "Game.h"
#include "Logger.h"
#include "GeneralUtils.h"
#include "BinaryIO.h"
#include "LUTriggers.h"

#include "AssetManager.h"
#include "CDClientManager.h"
#include "CDZoneTableTable.h"
#include "Spawner.h"
#include "dZoneManager.h"
#include "dpWorld.h"

#include "eTriggerCommandType.h"
#include "eTriggerEventType.h"
#include "dNavMesh.h"

Zone::Zone(const LWOMAPID& mapID, const LWOINSTANCEID& instanceID, const LWOCLONEID& cloneID) :
	m_ZoneID(mapID, instanceID, cloneID) {
	m_NumberOfObjectsLoaded = 0;
	m_NumberOfSceneTransitionsLoaded = 0;
	m_CheckSum = 0;
	m_WorldID = 0;
	m_SceneCount = 0;
}

Zone::~Zone() {
	LOG("Destroying zone %i", m_ZoneID.GetMapID());
	for (std::map<LWOSCENEID, SceneRef>::iterator it = m_Scenes.begin(); it != m_Scenes.end(); ++it) {
		if (it->second.level != nullptr) delete it->second.level;
	}
}

void Zone::Initalize() {
	LoadZoneIntoMemory();
	LoadLevelsIntoMemory();
	m_CheckSum = CalculateChecksum();
}

void Zone::LoadZoneIntoMemory() {
	m_ZoneFilePath = GetFilePathForZoneID();
	m_ZonePath = m_ZoneFilePath.substr(0, m_ZoneFilePath.rfind('/') + 1);
	if (m_ZoneFilePath == "ERR") return;

	auto file = Game::assetManager->GetFile(m_ZoneFilePath.c_str());

	if (!file) {
		LOG("Failed to load %s", m_ZoneFilePath.c_str());
		throw std::runtime_error("Aborting Zone loading due to no Zone File.");
	}

	if (file) {
		BinaryIO::BinaryRead(file, m_FileFormatVersion);

		uint32_t mapRevision = 0;
		if (m_FileFormatVersion >= Zone::FileFormatVersion::Alpha) BinaryIO::BinaryRead(file, mapRevision);

		BinaryIO::BinaryRead(file, m_WorldID);
		if (static_cast<LWOMAPID>(m_WorldID) != m_ZoneID.GetMapID()) LOG("WorldID: %i doesn't match MapID %i! Is this intended?", m_WorldID, m_ZoneID.GetMapID());

		AddRevision(LWOSCENEID_INVALID, mapRevision);

		if (m_FileFormatVersion >= Zone::FileFormatVersion::Beta) {
			BinaryIO::BinaryRead(file, m_Spawnpoint);
			BinaryIO::BinaryRead(file, m_SpawnpointRotation);
		}

		if (m_FileFormatVersion <= Zone::FileFormatVersion::LateAlpha) {
			uint8_t sceneCount;
			BinaryIO::BinaryRead(file, sceneCount);
			m_SceneCount = sceneCount;
		} else BinaryIO::BinaryRead(file, m_SceneCount);

		for (uint32_t i = 0; i < m_SceneCount; ++i) {
			LoadScene(file);
		}

		//Read generic zone info:
		BinaryIO::ReadString<uint8_t>(file, m_ZonePath, BinaryIO::ReadType::String);
		BinaryIO::ReadString<uint8_t>(file, m_ZoneRawPath, BinaryIO::ReadType::String);
		BinaryIO::ReadString<uint8_t>(file, m_ZoneName, BinaryIO::ReadType::String);
		BinaryIO::ReadString<uint8_t>(file, m_ZoneDesc, BinaryIO::ReadType::String);

		if (m_FileFormatVersion >= Zone::FileFormatVersion::PreAlpha) {
			BinaryIO::BinaryRead(file, m_NumberOfSceneTransitionsLoaded);
			for (uint32_t i = 0; i < m_NumberOfSceneTransitionsLoaded; ++i) {
				LoadSceneTransition(file);
			}
		}

		if (m_FileFormatVersion >= Zone::FileFormatVersion::EarlyAlpha) {
			BinaryIO::BinaryRead(file, m_PathDataLength);
			BinaryIO::BinaryRead(file, m_PathChunkVersion); // always should be 1

			uint32_t pathCount;
			BinaryIO::BinaryRead(file, pathCount);

			m_Paths.reserve(pathCount);
			for (uint32_t i = 0; i < pathCount; ++i) LoadPath(file);

			for (Path path : m_Paths) {
				if (path.pathType != PathType::Spawner) continue;
				SpawnerInfo info = SpawnerInfo();
				for (PathWaypoint waypoint : path.pathWaypoints) {
					SpawnerNode* node = new SpawnerNode();
					node->position = waypoint.position;
					node->rotation = waypoint.rotation;
					node->nodeID = 0;
					node->config = waypoint.config;

					for (LDFBaseData* data : waypoint.config) {
						if (!data) continue;

						if (data->GetKey() == u"spawner_node_id") {
							node->nodeID = std::stoi(data->GetValueAsString());
						} else if (data->GetKey() == u"spawner_max_per_node") {
							node->nodeMax = std::stoi(data->GetValueAsString());
						} else if (data->GetKey() == u"groupID") { // Load object group
							std::string groupStr = data->GetValueAsString();
							info.groups = GeneralUtils::SplitString(groupStr, ';');
							if (info.groups.back().empty()) info.groups.erase(info.groups.end() - 1);
						} else if (data->GetKey() == u"grpNameQBShowBricks") {
							if (data->GetValueAsString().empty()) continue;
							/*std::string groupStr = data->GetValueAsString();
							info.groups.push_back(groupStr);*/
							info.grpNameQBShowBricks = data->GetValueAsString();
						} else if (data->GetKey() == u"spawner_name") {
							info.name = data->GetValueAsString();
						}
					}
					info.nodes.push_back(node);
				}
				info.templateID = path.spawner.spawnedLOT;
				info.spawnerID = path.spawner.spawnerObjID;
				info.respawnTime = path.spawner.respawnTime;
				info.amountMaintained = path.spawner.amountMaintained;
				info.maxToSpawn = path.spawner.maxToSpawn;
				info.activeOnLoad = path.spawner.spawnerNetActive;
				info.isNetwork = true;
				Spawner* spawner = new Spawner(info);
				Game::zoneManager->AddSpawner(info.spawnerID, spawner);
			}
		}
	} else {
		LOG("Failed to open: %s", m_ZoneFilePath.c_str());
	}

	m_ZonePath = m_ZoneFilePath.substr(0, m_ZoneFilePath.rfind('/') + 1);
}

std::string Zone::GetFilePathForZoneID() {
	//We're gonna go ahead and presume we've got the db loaded already:
	CDZoneTableTable* zoneTable = CDClientManager::GetTable<CDZoneTableTable>();
	const CDZoneTable* zone = zoneTable->Query(this->GetZoneID().GetMapID());
	if (zone != nullptr) {
		std::string toReturn = "maps/" + zone->zoneName;
		std::transform(toReturn.begin(), toReturn.end(), toReturn.begin(), ::tolower);
		return toReturn;
	}

	return std::string("ERR");
}

//Based off code from: https://www.liquisearch.com/fletchers_checksum/implementation/optimizations
uint32_t Zone::CalculateChecksum() {
	uint32_t sum1 = 0xffff, sum2 = 0xffff;

	for (const auto& [scene, sceneRevision] : m_MapRevisions) {
		uint32_t sceneID = scene.GetSceneID();
		sum2 += sum1 += (sceneID >> 16);
		sum2 += sum1 += (sceneID & 0xffff);

		uint32_t layerID = scene.GetLayerID();
		sum2 += sum1 += (layerID >> 16);
		sum2 += sum1 += (layerID & 0xffff);

		uint32_t revision = sceneRevision;
		sum2 += sum1 += (revision >> 16);
		sum2 += sum1 += (revision & 0xffff);
	}

	sum1 = (sum1 & 0xffff) + (sum1 >> 16);
	sum2 = (sum2 & 0xffff) + (sum2 >> 16);

	return sum2 << 16 | sum1;
}

void Zone::LoadLevelsIntoMemory() {
	for (auto& [sceneID, scene] : m_Scenes) {
		if (scene.level) continue;
		scene.level = new Level(this, m_ZonePath + scene.filename);

		if (scene.level->m_ChunkHeaders.empty()) continue;

		scene.level->m_ChunkHeaders.begin()->second.lwoSceneID = sceneID;
		AddRevision(scene.level->m_ChunkHeaders.begin()->second.lwoSceneID, scene.level->m_ChunkHeaders.begin()->second.fileInfo.revision);
	}
}

void Zone::AddRevision(LWOSCENEID sceneID, uint32_t revision) {
	if (m_MapRevisions.find(sceneID) == m_MapRevisions.end()) {
		m_MapRevisions.insert(std::make_pair(sceneID, revision));
	}
}

void Zone::LoadScene(std::istream& file) {
	SceneRef scene;
	scene.level = nullptr;
	LWOSCENEID lwoSceneID(LWOZONEID_INVALID, 0);

	BinaryIO::ReadString<uint8_t>(file, scene.filename, BinaryIO::ReadType::String);

	std::string luTriggersPath = scene.filename.substr(0, scene.filename.size() - 4) + ".lutriggers";
	if (Game::assetManager->HasFile((m_ZonePath + luTriggersPath).c_str())) LoadLUTriggers(luTriggersPath, scene);

	if (m_FileFormatVersion >= Zone::FileFormatVersion::LatePreAlpha || m_FileFormatVersion < Zone::FileFormatVersion::PrePreAlpha) {
		BinaryIO::BinaryRead(file, scene.id);
		lwoSceneID.SetSceneID(scene.id);
	}
	if (m_FileFormatVersion >= Zone::FileFormatVersion::LatePreAlpha) {
		BinaryIO::BinaryRead(file, scene.sceneType);
		lwoSceneID.SetLayerID(scene.sceneType);

		BinaryIO::ReadString<uint8_t>(file, scene.name, BinaryIO::ReadType::String);
	}

	if (m_FileFormatVersion == Zone::FileFormatVersion::LatePreAlpha) {
		BinaryIO::BinaryRead(file, scene.unknown1);
		BinaryIO::BinaryRead(file, scene.unknown2);
	}

	if (m_FileFormatVersion >= Zone::FileFormatVersion::LatePreAlpha) {
		BinaryIO::BinaryRead(file, scene.color_r);
		BinaryIO::BinaryRead(file, scene.color_b);
		BinaryIO::BinaryRead(file, scene.color_g);
	}

	m_Scenes.insert(std::make_pair(lwoSceneID, scene));
}

void Zone::LoadLUTriggers(std::string triggerFile, SceneRef& scene) {
	auto file = Game::assetManager->GetFile((m_ZonePath + triggerFile).c_str());

	std::stringstream data;
	data << file.rdbuf();

	data.seekg(0, std::ios::end);
	int32_t size = data.tellg();
	data.seekg(0, std::ios::beg);

	if (size == 0) return;

	tinyxml2::XMLDocument doc;

	if (doc.Parse(data.str().c_str(), size) != tinyxml2::XML_SUCCESS) {
		LOG("Failed to load LUTriggers from file %s", triggerFile.c_str());
		return;
	}

	auto* triggers = doc.FirstChildElement("triggers");
	if (!triggers) return;

	auto* currentTrigger = triggers->FirstChildElement("trigger");
	while (currentTrigger) {
		LUTriggers::Trigger* newTrigger = new LUTriggers::Trigger();
		currentTrigger->QueryAttribute("enabled", &newTrigger->enabled);
		currentTrigger->QueryAttribute("id", &newTrigger->id);

		auto* currentEvent = currentTrigger->FirstChildElement("event");
		while (currentEvent) {
			LUTriggers::Event* newEvent = new LUTriggers::Event();
			newEvent->id = TriggerEventType::StringToTriggerEventType(currentEvent->Attribute("id"));
			auto* currentCommand = currentEvent->FirstChildElement("command");
			while (currentCommand) {
				LUTriggers::Command* newCommand = new LUTriggers::Command();
				newCommand->id = TriggerCommandType::StringToTriggerCommandType(currentCommand->Attribute("id"));
				newCommand->target = currentCommand->Attribute("target");
				if (currentCommand->Attribute("targetName")) {
					newCommand->targetName = currentCommand->Attribute("targetName");
				}
				if (currentCommand->Attribute("args")) {
					newCommand->args = currentCommand->Attribute("args");
				}

				newEvent->commands.push_back(newCommand);
				currentCommand = currentCommand->NextSiblingElement("command");
			}
			newTrigger->events.push_back(newEvent);
			currentEvent = currentEvent->NextSiblingElement("event");
		}
		currentTrigger = currentTrigger->NextSiblingElement("trigger");
		scene.triggers.insert(std::make_pair(newTrigger->id, newTrigger));
	}
}

LUTriggers::Trigger* Zone::GetTrigger(uint32_t sceneID, uint32_t triggerID) {
	auto scene = m_Scenes.find(sceneID);
	if (scene == m_Scenes.end()) return nullptr;

	auto trigger = scene->second.triggers.find(triggerID);
	if (trigger == scene->second.triggers.end()) return nullptr;

	return trigger->second;
}

const Path* Zone::GetPath(std::string name) const {
	for (const auto& path : m_Paths) {
		if (name == path.pathName) {
			return &path;
		}
	}

	return nullptr;
}

void Zone::LoadSceneTransition(std::istream& file) {
	SceneTransition sceneTrans;
	if (m_FileFormatVersion < Zone::FileFormatVersion::Auramar) {
		BinaryIO::ReadString<uint8_t>(file, sceneTrans.name, BinaryIO::ReadType::String);
		BinaryIO::BinaryRead(file, sceneTrans.width);
	}

	//BR�THER MAY I HAVE SOME L��PS?
	uint8_t loops = (m_FileFormatVersion <= Zone::FileFormatVersion::LatePreAlpha || m_FileFormatVersion >= Zone::FileFormatVersion::Launch) ? 2 : 5;

	sceneTrans.points.reserve(loops);
	for (uint8_t i = 0; i < loops; ++i) {
		sceneTrans.points.push_back(LoadSceneTransitionInfo(file));
	}

	m_SceneTransitions.push_back(sceneTrans);
}

SceneTransitionInfo Zone::LoadSceneTransitionInfo(std::istream& file) {
	SceneTransitionInfo info;
	BinaryIO::BinaryRead(file, info.sceneID);
	BinaryIO::BinaryRead(file, info.position);
	return info;
}

void Zone::LoadPath(std::istream& file) {
	Path path = Path();

	BinaryIO::BinaryRead(file, path.pathVersion);

	BinaryIO::ReadString<uint8_t>(file, path.pathName, BinaryIO::ReadType::WideString);

	BinaryIO::BinaryRead(file, path.pathType);
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
			uint32_t count;
			BinaryIO::BinaryRead(file, count);
			for (uint32_t i = 0; i < count; ++i) {
				std::string parameter;
				BinaryIO::ReadString<uint8_t>(file, parameter, BinaryIO::ReadType::WideString);

				std::string value;
				BinaryIO::ReadString<uint8_t>(file, value, BinaryIO::ReadType::WideString);

				LDFBaseData* ldfConfig = nullptr;
				if (path.pathType == PathType::Movement || path.pathType == PathType::Rail) {
					ldfConfig = LDFBaseData::DataFromString(parameter + "=0:" + value);
				} else {
					ldfConfig = LDFBaseData::DataFromString(parameter + "=" + value);
				}
				if (ldfConfig) waypoint.config.push_back(ldfConfig);
			}
		}
		// We verify the waypoint heights against the navmesh because in many movement paths,
		// the waypoint is located near 0 height, 
		if (path.pathType == PathType::Movement) {
			if (dpWorld::IsLoaded()) {
				// 2000 should be large enough for every world.
				waypoint.position.y = dpWorld::GetNavMesh()->GetHeightAtPoint(waypoint.position, 2000.0f);
			}
		}
		path.pathWaypoints.push_back(waypoint);
	}
	m_Paths.push_back(path);
}
