#include "Zone.h"
#include "Level.h"
#include <fstream>
#include <sstream>
#include "Game.h"
#include "dLogger.h"
#include "GeneralUtils.h"
#include "BinaryIO.h"

#include "CDClientManager.h"
#include "CDZoneTableTable.h"
#include "Spawner.h"
#include "dZoneManager.h"

Zone::Zone(const LWOMAPID& mapID, const LWOINSTANCEID& instanceID, const LWOCLONEID& cloneID) :
	m_ZoneID(mapID, instanceID, cloneID) {
	m_NumberOfScenesLoaded = 0;
	m_NumberOfObjectsLoaded = 0;
	m_NumberOfSceneTransitionsLoaded = 0;
	m_CheckSum = 0;
	m_WorldID = 0;
	m_SceneCount = 0;
}

Zone::~Zone() {
	Game::logger->Log("Zone", "Destroying zone %i", m_ZoneID.GetMapID());
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

	std::ifstream file(m_ZoneFilePath, std::ios::binary);
	if (file) {
		BinaryIO::BinaryRead(file, m_ZoneFileFormatVersion);

		uint32_t mapRevision = 0;
		if (m_ZoneFileFormatVersion >= Zone::ZoneFileFormatVersion::Alpha) BinaryIO::BinaryRead(file, mapRevision);

		BinaryIO::BinaryRead(file, m_WorldID);
		if ((uint16_t)m_WorldID != m_ZoneID.GetMapID()) Game::logger->Log("Zone", "WorldID: %i doesn't match MapID %i! Is this intended?", m_WorldID, m_ZoneID.GetMapID());

		AddRevision(LWOSCENEID_INVALID, mapRevision);

		if (m_ZoneFileFormatVersion >= Zone::ZoneFileFormatVersion::Beta) {
			BinaryIO::BinaryRead(file, m_Spawnpoint);
			BinaryIO::BinaryRead(file, m_SpawnpointRotation);
		}

		if (m_ZoneFileFormatVersion <= Zone::ZoneFileFormatVersion::LateAlpha) {
			uint8_t sceneCount;
			BinaryIO::BinaryRead(file, sceneCount);
			m_SceneCount = sceneCount;
		} else BinaryIO::BinaryRead(file, m_SceneCount);

		for (uint32_t i = 0; i < m_SceneCount; ++i) {
			LoadScene(file);
		}

		//Read generic zone info:
		uint8_t stringLength;
		BinaryIO::BinaryRead(file, stringLength);
		m_ZonePath = BinaryIO::ReadString(file, stringLength);

		BinaryIO::BinaryRead(file, stringLength);
		m_ZoneRawPath = BinaryIO::ReadString(file, stringLength);

		BinaryIO::BinaryRead(file, stringLength);
		m_ZoneName = BinaryIO::ReadString(file, stringLength);

		BinaryIO::BinaryRead(file, stringLength);
		m_ZoneDesc = BinaryIO::ReadString(file, stringLength);

		if (m_ZoneFileFormatVersion >= Zone::ZoneFileFormatVersion::PreAlpha) {
			BinaryIO::BinaryRead(file, m_NumberOfSceneTransitionsLoaded);
			for (uint32_t i = 0; i < m_NumberOfSceneTransitionsLoaded; ++i) {
				LoadSceneTransition(file);
			}
		}

		if (m_ZoneFileFormatVersion >= Zone::ZoneFileFormatVersion::EarlyAlpha) {
			BinaryIO::BinaryRead(file, m_PathDataLength);
			uint32_t unknown;
			uint32_t pathCount;

			BinaryIO::BinaryRead(file, unknown);
			BinaryIO::BinaryRead(file, pathCount);

			for (uint32_t i = 0; i < pathCount; ++i) {
				LoadPath(file);
			}

			for (Path path : m_Paths) {
				if (path.pathType == PathType::Spawner) {
					SpawnerInfo info = SpawnerInfo();
					for (PathWaypoint waypoint : path.pathWaypoints) {
						SpawnerNode* node = new SpawnerNode();
						node->position = waypoint.position;
						node->rotation = waypoint.rotation;
						node->nodeID = 0;
						node->config = waypoint.config;

						for (LDFBaseData* data : waypoint.config) {
							if (data) {
								if (data->GetKey() == u"spawner_node_id") {
									node->nodeID = std::stoi(data->GetValueAsString());
								} else if (data->GetKey() == u"spawner_max_per_node") {
									node->nodeMax = std::stoi(data->GetValueAsString());
								} else if (data->GetKey() == u"groupID") { // Load object group
									std::string groupStr = data->GetValueAsString();
									info.groups = GeneralUtils::SplitString(groupStr, ';');
									info.groups.erase(info.groups.end() - 1);
								} else if (data->GetKey() == u"grpNameQBShowBricks") {
									if (data->GetValueAsString() == "") continue;
									/*std::string groupStr = data->GetValueAsString();
									info.groups.push_back(groupStr);*/
									info.grpNameQBShowBricks = data->GetValueAsString();
								} else if (data->GetKey() == u"spawner_name") {
									info.name = data->GetValueAsString();
								}
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
					dZoneManager::Instance()->AddSpawner(info.spawnerID, spawner);
				}

			}


			//m_PathData.resize(m_PathDataLength);
			//file.read((char*)&m_PathData[0], m_PathDataLength);
		}
	} else {
		Game::logger->Log("Zone", "Failed to open: %s", m_ZoneFilePath.c_str());
	}
	m_ZonePath = m_ZoneFilePath.substr(0, m_ZoneFilePath.rfind('/') + 1);

	file.close();
}

std::string Zone::GetFilePathForZoneID() {
	//We're gonna go ahead and presume we've got the db loaded already:
	CDZoneTableTable* zoneTable = CDClientManager::Instance()->GetTable<CDZoneTableTable>("ZoneTable");
	const CDZoneTable* zone = zoneTable->Query(this->GetZoneID().GetMapID());
	if (zone != nullptr) {
		std::string toReturn = "./res/maps/" + zone->zoneName;
		std::transform(toReturn.begin(), toReturn.end(), toReturn.begin(), ::tolower);
		return toReturn;
	}

	return std::string("ERR");
}

//Based off code from: https://www.liquisearch.com/fletchers_checksum/implementation/optimizations
uint32_t Zone::CalculateChecksum() {
	uint32_t sum1 = 0xffff, sum2 = 0xffff;

	for (std::map<LWOSCENEID, uint32_t>::const_iterator it = m_MapRevisions.cbegin(); it != m_MapRevisions.cend(); ++it) {
		uint32_t sceneID = it->first.GetSceneID();
		sum2 += sum1 += (sceneID >> 16);
		sum2 += sum1 += (sceneID & 0xffff);

		uint32_t layerID = it->first.GetLayerID();
		sum2 += sum1 += (layerID >> 16);
		sum2 += sum1 += (layerID & 0xffff);

		uint32_t revision = it->second;
		sum2 += sum1 += (revision >> 16);
		sum2 += sum1 += (revision & 0xffff);
	}

	sum1 = (sum1 & 0xffff) + (sum1 >> 16);
	sum2 = (sum2 & 0xffff) + (sum2 >> 16);

	return sum2 << 16 | sum1;
}

void Zone::LoadLevelsIntoMemory() {
	for (std::map<LWOSCENEID, SceneRef>::iterator it = m_Scenes.begin(); it != m_Scenes.end(); ++it) {
		if (it->second.level == nullptr) {
			it->second.level = new Level(this, m_ZonePath + it->second.filename);

			if (it->second.level->m_ChunkHeaders.size() > 0) {
				it->second.level->m_ChunkHeaders.begin()->second.lwoSceneID = it->first;
				AddRevision(it->second.level->m_ChunkHeaders.begin()->second.lwoSceneID, it->second.level->m_ChunkHeaders.begin()->second.fileInfo->revision);
			}
		}
	}
}

void Zone::AddRevision(LWOSCENEID sceneID, uint32_t revision) {
	for (std::pair<LWOSCENEID, uint32_t> item : m_MapRevisions) {
		if (item.first == sceneID) return;
	}

	m_MapRevisions[LWOSCENEID(sceneID)] = revision;
}

const void Zone::PrintAllGameObjects() {
	for (std::pair<LWOSCENEID, SceneRef> scene : m_Scenes) {
		Game::logger->Log("Zone", "In sceneID: %i", scene.first.GetSceneID());
		scene.second.level->PrintAllObjects();
	}
}

void Zone::LoadScene(std::ifstream& file) {
	SceneRef scene;
	scene.level = nullptr;
	LWOSCENEID lwoSceneID(LWOZONEID_INVALID, 0);

	uint8_t sceneFilenameLength;
	BinaryIO::BinaryRead(file, sceneFilenameLength);
	scene.filename = BinaryIO::ReadString(file, sceneFilenameLength);

	std::string luTriggersPath = scene.filename.substr(0, scene.filename.size() - 4) + ".lutriggers";
	std::vector<LUTriggers::Trigger*> triggers = LoadLUTriggers(luTriggersPath, scene.id);

	for (LUTriggers::Trigger* trigger : triggers) {
		scene.triggers.insert({ trigger->id, trigger });
	}

	BinaryIO::BinaryRead(file, scene.id);
	BinaryIO::BinaryRead(file, scene.sceneType);
	lwoSceneID.SetSceneID(scene.id);

	uint8_t sceneNameLength;
	BinaryIO::BinaryRead(file, sceneNameLength);
	scene.name = BinaryIO::ReadString(file, sceneNameLength);
	file.ignore(3);

	/*
	if (m_Scenes.find(scene.id) != m_Scenes.end()) {
		//Extract the layer id from the filename (bad I know, but it's reliable at least):
		std::string layer = scene.filename.substr(scene.filename.rfind('x') + 1);
		layer = layer.substr(0, layer.find('_'));
		lwoSceneID.SetLayerID(std::atoi(layer.c_str()));
	}
	*/

	lwoSceneID.SetLayerID(scene.sceneType);

	m_Scenes.insert(std::make_pair(lwoSceneID, scene));
	m_NumberOfScenesLoaded++;
}

std::vector<LUTriggers::Trigger*> Zone::LoadLUTriggers(std::string triggerFile, LWOSCENEID sceneID) {
	std::vector<LUTriggers::Trigger*> lvlTriggers;
	std::ifstream file(m_ZonePath + triggerFile);
	std::stringstream data;
	data << file.rdbuf();

	if (data.str().size() == 0) return lvlTriggers;

	tinyxml2::XMLDocument* doc = new tinyxml2::XMLDocument();
	if (!doc) return lvlTriggers;

	if (doc->Parse(data.str().c_str(), data.str().size()) == 0) {
		//Game::logger->Log("Zone", "Loaded LUTriggers from file %s!", triggerFile.c_str());
	} else {
		Game::logger->Log("Zone", "Failed to load LUTriggers from file %s", triggerFile.c_str());
		return lvlTriggers;
	}

	tinyxml2::XMLElement* triggers = doc->FirstChildElement("triggers");
	if (!triggers) return lvlTriggers;

	auto currentTrigger = triggers->FirstChildElement("trigger");
	while (currentTrigger) {
		LUTriggers::Trigger* newTrigger = new LUTriggers::Trigger();
		currentTrigger->QueryAttribute("enabled", &newTrigger->enabled);
		currentTrigger->QueryAttribute("id", &newTrigger->id);

		auto currentEvent = currentTrigger->FirstChildElement("event");
		while (currentEvent) {
			LUTriggers::Event* newEvent = new LUTriggers::Event();
			newEvent->eventID = currentEvent->Attribute("id");
			auto currentCommand = currentEvent->FirstChildElement("command");
			while (currentCommand) {
				LUTriggers::Command* newCommand = new LUTriggers::Command();
				newCommand->id = currentCommand->Attribute("id");
				newCommand->target = currentCommand->Attribute("target");
				if (currentCommand->Attribute("targetName") != NULL) {
					newCommand->targetName = currentCommand->Attribute("targetName");
				}
				if (currentCommand->Attribute("args") != NULL) {
					newCommand->args = currentCommand->Attribute("args");
				}
				newEvent->commands.push_back(newCommand);
				currentCommand = currentCommand->NextSiblingElement("command");
			}
			newTrigger->events.push_back(newEvent);
			currentEvent = currentEvent->NextSiblingElement("event");
		}
		currentTrigger = currentTrigger->NextSiblingElement("trigger");
		lvlTriggers.push_back(newTrigger);
	}

	delete doc;

	return lvlTriggers;
}

LUTriggers::Trigger* Zone::GetTrigger(uint32_t sceneID, uint32_t triggerID) {
	if (m_Scenes.find(sceneID) == m_Scenes.end()) return nullptr;
	if (m_Scenes[sceneID].triggers.find(triggerID) == m_Scenes[sceneID].triggers.end()) return nullptr;

	return m_Scenes[sceneID].triggers[triggerID];
}

const Path* Zone::GetPath(std::string name) const {
	for (const auto& path : m_Paths) {
		if (name == path.pathName) {
			return &path;
		}
	}

	return nullptr;
}

void Zone::LoadSceneTransition(std::ifstream& file) {
	SceneTransition sceneTrans;
	if (m_ZoneFileFormatVersion < Zone::ZoneFileFormatVersion::LateAlpha) {
		uint8_t length;
		BinaryIO::BinaryRead(file, length);
		sceneTrans.name = BinaryIO::ReadString(file, length);
	}

	//BR�THER MAY I HAVE SOME L��PS?
	uint8_t loops = (m_ZoneFileFormatVersion < Zone::ZoneFileFormatVersion::EarlyAlpha || m_ZoneFileFormatVersion >= Zone::ZoneFileFormatVersion::Launch) ? 2 : 5;

	for (uint8_t i = 0; i < loops; ++i) {
		sceneTrans.points.push_back(LoadSceneTransitionInfo(file));
	}

	m_SceneTransitions.push_back(sceneTrans);
}

SceneTransitionInfo Zone::LoadSceneTransitionInfo(std::ifstream& file) {
	SceneTransitionInfo info;
	BinaryIO::BinaryRead(file, info.sceneID);
	BinaryIO::BinaryRead(file, info.position);
	return info;
}

void Zone::LoadPath(std::ifstream& file) {
	// Currently only spawner (type 4) paths are supported
	Path path = Path();

	uint32_t unknown1;
	uint32_t pathType;
	uint32_t pathBehavior;

	BinaryIO::BinaryRead(file, path.pathVersion);
	uint8_t stringLength;
	BinaryIO::BinaryRead(file, stringLength);
	for (uint8_t i = 0; i < stringLength; ++i) {
		uint16_t character;
		BinaryIO::BinaryRead(file, character);
		path.pathName.push_back(character);
	}
	BinaryIO::BinaryRead(file, pathType);
	path.pathType = PathType(pathType);
	BinaryIO::BinaryRead(file, unknown1);
	BinaryIO::BinaryRead(file, pathBehavior);
	path.pathType = PathType(pathType);

	if (path.pathType == PathType::MovingPlatform) {
		if (path.pathVersion >= 18) {
			uint8_t unknown;
			BinaryIO::BinaryRead(file, unknown);
		} else if (path.pathVersion >= 13) {
			uint8_t count;
			BinaryIO::BinaryRead(file, count);
			for (uint8_t i = 0; i < count; ++i) {
				uint16_t character;
				BinaryIO::BinaryRead(file, character);
				path.movingPlatform.platformTravelSound.push_back(character);
			}
		}
	} else if (path.pathType == PathType::Property) {
		int32_t unknown;
		BinaryIO::BinaryRead(file, unknown);
		BinaryIO::BinaryRead(file, path.property.price);
		BinaryIO::BinaryRead(file, path.property.rentalTime);
		BinaryIO::BinaryRead(file, path.property.associatedZone);
		uint8_t count1;
		BinaryIO::BinaryRead(file, count1);
		for (uint8_t i = 0; i < count1; ++i) {
			uint16_t character;
			BinaryIO::BinaryRead(file, character);
			path.property.displayName.push_back(character);
		}
		uint32_t count2;
		BinaryIO::BinaryRead(file, count2);
		for (uint8_t i = 0; i < count2; ++i) {
			uint16_t character;
			BinaryIO::BinaryRead(file, character);
			path.property.displayDesc.push_back(character);
		}
		int32_t unknown1;
		BinaryIO::BinaryRead(file, unknown1);
		BinaryIO::BinaryRead(file, path.property.cloneLimit);
		BinaryIO::BinaryRead(file, path.property.repMultiplier);
		BinaryIO::BinaryRead(file, path.property.rentalTimeUnit);
		BinaryIO::BinaryRead(file, path.property.achievementRequired);
		BinaryIO::BinaryRead(file, path.property.playerZoneCoords.x);
		BinaryIO::BinaryRead(file, path.property.playerZoneCoords.y);
		BinaryIO::BinaryRead(file, path.property.playerZoneCoords.z);
		BinaryIO::BinaryRead(file, path.property.maxBuildHeight);
	} else if (path.pathType == PathType::Camera) {
		uint8_t count;
		BinaryIO::BinaryRead(file, count);
		for (uint8_t i = 0; i < count; ++i) {
			uint16_t character;
			BinaryIO::BinaryRead(file, character);
			path.camera.nextPath.push_back(character);
		}
		if (path.pathVersion >= 14) {
			uint8_t unknown;
			BinaryIO::BinaryRead(file, unknown);
		}
	} else if (path.pathType == PathType::Spawner) {
		//SpawnerPath* path = static_cast<SpawnerPath*>(path); // Convert to a spawner path
		BinaryIO::BinaryRead(file, path.spawner.spawnedLOT);
		BinaryIO::BinaryRead(file, path.spawner.respawnTime);
		BinaryIO::BinaryRead(file, path.spawner.maxToSpawn);
		BinaryIO::BinaryRead(file, path.spawner.amountMaintained);
		BinaryIO::BinaryRead(file, path.spawner.spawnerObjID);
		BinaryIO::BinaryRead(file, path.spawner.spawnerNetActive);
	}

	// Read waypoints

	BinaryIO::BinaryRead(file, path.waypointCount);

	for (uint32_t i = 0; i < path.waypointCount; ++i) {
		PathWaypoint waypoint = PathWaypoint();

		BinaryIO::BinaryRead(file, waypoint.position.x);
		BinaryIO::BinaryRead(file, waypoint.position.y);
		BinaryIO::BinaryRead(file, waypoint.position.z);


		if (path.pathType == PathType::Spawner || path.pathType == PathType::MovingPlatform || path.pathType == PathType::Race) {
			BinaryIO::BinaryRead(file, waypoint.rotation.w);
			BinaryIO::BinaryRead(file, waypoint.rotation.x);
			BinaryIO::BinaryRead(file, waypoint.rotation.y);
			BinaryIO::BinaryRead(file, waypoint.rotation.z);
		}

		if (path.pathType == PathType::MovingPlatform) {
			BinaryIO::BinaryRead(file, waypoint.movingPlatform.lockPlayer);
			BinaryIO::BinaryRead(file, waypoint.movingPlatform.speed);
			BinaryIO::BinaryRead(file, waypoint.movingPlatform.wait);
			if (path.pathVersion >= 13) {
				uint8_t count1;
				BinaryIO::BinaryRead(file, count1);
				for (uint8_t i = 0; i < count1; ++i) {
					uint16_t character;
					BinaryIO::BinaryRead(file, character);
					waypoint.movingPlatform.departSound.push_back(character);
				}
				uint8_t count2;
				BinaryIO::BinaryRead(file, count2);
				for (uint8_t i = 0; i < count2; ++i) {
					uint16_t character;
					BinaryIO::BinaryRead(file, character);
					waypoint.movingPlatform.arriveSound.push_back(character);
				}
			}
		} else if (path.pathType == PathType::Camera) {
			float unknown;
			BinaryIO::BinaryRead(file, unknown);
			BinaryIO::BinaryRead(file, unknown);
			BinaryIO::BinaryRead(file, unknown);
			BinaryIO::BinaryRead(file, unknown);
			BinaryIO::BinaryRead(file, waypoint.camera.time);
			BinaryIO::BinaryRead(file, unknown);
			BinaryIO::BinaryRead(file, waypoint.camera.tension);
			BinaryIO::BinaryRead(file, waypoint.camera.continuity);
			BinaryIO::BinaryRead(file, waypoint.camera.bias);
		} else if (path.pathType == PathType::Race) {
			uint8_t unknown;
			BinaryIO::BinaryRead(file, unknown);
			BinaryIO::BinaryRead(file, unknown);
			float unknown1;
			BinaryIO::BinaryRead(file, unknown1);
			BinaryIO::BinaryRead(file, unknown1);
			BinaryIO::BinaryRead(file, unknown1);
		} else if (path.pathType == PathType::Rail) {
			float unknown;
			BinaryIO::BinaryRead(file, unknown);
			BinaryIO::BinaryRead(file, unknown);
			BinaryIO::BinaryRead(file, unknown);
			BinaryIO::BinaryRead(file, unknown);
			if (path.pathVersion >= 17) {
				BinaryIO::BinaryRead(file, unknown);
			}
		}

		// object LDF configs
		if (path.pathType == PathType::Movement || path.pathType == PathType::Spawner || path.pathType == PathType::Rail) {
			uint32_t count;
			BinaryIO::BinaryRead(file, count);
			for (uint32_t i = 0; i < count; ++i) {
				uint8_t count1;
				std::string parameter;
				std::string value;
				BinaryIO::BinaryRead(file, count1);
				for (uint8_t i = 0; i < count1; ++i) {
					uint16_t character;
					BinaryIO::BinaryRead(file, character);
					parameter.push_back(character);
				}
				uint8_t count2;
				BinaryIO::BinaryRead(file, count2);
				for (uint8_t i = 0; i < count2; ++i) {
					uint16_t character;
					BinaryIO::BinaryRead(file, character);
					value.push_back(character);
				}
				LDFBaseData* ldfConfig = LDFBaseData::DataFromString(parameter + "=" + value);
				waypoint.config.push_back(ldfConfig);
			}
		}

		path.pathWaypoints.push_back(waypoint);
	}



	m_Paths.push_back(path);
}
