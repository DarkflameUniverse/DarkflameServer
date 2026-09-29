#include "Zone.h"
#include "Level.h"
#include <fstream>
#include <sstream>
#include <ranges>
#include "Game.h"
#include "Logger.h"
#include "GeneralUtils.h"
#include "BinaryIO.h"
#include "LUTriggers.h"

#include "AssetManager.h"
#include "CDClientManager.h"
#include "CDZoneTableTable.h"
#include "CDClientDatabase.h"
#include "Spawner.h"
#include "dZoneManager.h"
#include "dpWorld.h"

#include "eTriggerCommandType.h"
#include "eTriggerEventType.h"
#include "eWaypointCommandType.h"
#include "dNavMesh.h"

Zone::Zone(const LWOZONEID zoneID) :
	m_ZoneID(zoneID) {
	m_NumberOfObjectsLoaded = 0;
	m_NumberOfSceneTransitionsLoaded = 0;
	m_CheckSum = 0;
	m_WorldID = 0;
	m_SceneCount = 0;
}

Zone::~Zone() {
	LOG("Destroying zone %i", m_ZoneID.GetMapID());
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
		// The file itself is read by ZoneFile (dCommon), which the dashboard uses too
		ZoneFile zoneFile;
		zoneFile.Read(file);
		// The oldest files name their scenes by SceneTable ID
		zoneFile.ResolveSceneTable([](uint32_t sceneTableID) -> std::optional<std::string> {
			auto query = CDClientDatabase::CreatePreppedStmt("SELECT sceneName FROM SceneTable WHERE sceneID = ?;");
			query.bind(1, static_cast<int32_t>(sceneTableID));
			auto result = query.execQuery();
			if (result.eof() || result.fieldIsNull("sceneName")) return std::nullopt;
			return std::string(result.getStringField("sceneName"));
		});
		m_FileFormatVersion = zoneFile.fileFormatVersion;
		m_WorldID = zoneFile.worldID;
		if (static_cast<LWOMAPID>(m_WorldID) != m_ZoneID.GetMapID()) LOG("WorldID: %i doesn't match MapID %i! Is this intended?", m_WorldID, m_ZoneID.GetMapID());

		AddRevision(LWOSCENEID_INVALID, zoneFile.mapRevision);

		m_Spawnpoint = zoneFile.spawnpoint;
		m_SpawnpointRotation = zoneFile.spawnpointRotation;

		m_SceneCount = zoneFile.scenes.size();
		m_SceneGraph = ZoneScenes::SceneGraph(zoneFile.scenes, zoneFile.sceneTransitions);
		for (auto& scene : zoneFile.scenes) {
			LoadScene(std::move(scene));
		}

		m_ZoneRawPath = zoneFile.zoneRawPath;
		m_ZoneName = zoneFile.zoneName;
		m_ZoneDesc = zoneFile.zoneDesc;

		m_SceneTransitions = std::move(zoneFile.sceneTransitions);
		m_NumberOfSceneTransitionsLoaded = m_SceneTransitions.size();

		m_PathDataLength = zoneFile.pathDataLength;
		m_PathChunkVersion = zoneFile.pathChunkVersion;
		m_Paths = std::move(zoneFile.paths);

		// We verify the waypoint heights against the navmesh because in many movement paths,
		// the waypoint is located near 0 height, 
		if (dpWorld::IsLoaded()) {
			for (auto& path : m_Paths) {
				if (path.pathType != PathType::Movement) continue;
				for (auto& waypoint : path.pathWaypoints) {
					// 2000 should be large enough for every world.
					waypoint.position.y = dpWorld::GetNavMesh()->GetHeightAtPoint(waypoint.position, 2000.0f);
				}
			}
		}

		if (m_FileFormatVersion >= Zone::FileFormatVersion::EarlyAlpha) {
			for (const Path& path : m_Paths) {
				if (path.pathType != PathType::Spawner) continue;
				SpawnerInfo info{};
				for (size_t i = 0; i < path.pathWaypoints.size(); i++) {
					const auto& waypoint = path.pathWaypoints[i];
					SpawnerNode* node = new SpawnerNode();
					node->position = waypoint.position;
					node->rotation = waypoint.rotation;
					node->nodeID = 0;
					node->config = path.pathWaypoints[0].config;
					// All spawner waypoints get the config data of the first waypoint, but then we
					// overwrite settings on this waypoint if we have another one defined of the same name
					if (i != 0) {
						for (const auto& [key, value] : waypoint.config) {
							node->config.ParseInsert(value->GetString());
						}
					}

					for (const auto& data : waypoint.config | std::views::values) {
						if (!data) continue;

						if (data->GetKey() == u"spawner_node_id") {
							node->nodeID = GeneralUtils::TryParse(data->GetValueAsString(), 0);
						} else if (data->GetKey() == u"spawner_max_per_node") {
							node->nodeMax = GeneralUtils::TryParse(data->GetValueAsString(), 0);
						} else if (data->GetKey() == u"groupID") { // Load object group
							info.groups = GeneralUtils::SplitString(data->GetValueAsString(), ';');
							if (info.groups.back().empty()) info.groups.erase(info.groups.end() - 1);
						} else if (data->GetKey() == u"grpNameQBShowBricks") {
							info.grpNameQBShowBricks = data->GetValueAsString();
						} else if (data->GetKey() == u"spawner_name") {
							info.name = data->GetValueAsString();
						} else if (data->GetKey() == u"weight") {
							node->weight = GeneralUtils::TryParse(data->GetValueAsString(), 1);
							if (node->weight <= 0) {
								LOG("Found a spawner with a weight of <= 0, is this intentional? %s:%i", info.name.c_str(), node->nodeID);
								node->weight = 1;
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
				Game::zoneManager->AddSpawner(info.spawnerID, spawner);
			}
		}
	} else {
		LOG("Failed to open: %s", m_ZoneFilePath.c_str());
	}

	m_ZonePath = m_ZoneFilePath.substr(0, m_ZoneFilePath.rfind('/') + 1);
}

std::string Zone::GetFilePathForZoneID() const {
	//We're gonna go ahead and presume we've got the db loaded already:
	const CDZoneTable* zone = CDZoneTableTable::Query(this->GetZoneID().GetMapID());
	std::string toReturn("ERR");
	if (zone != nullptr) {
		toReturn = "maps/" + zone->zoneName;
		std::transform(toReturn.begin(), toReturn.end(), toReturn.begin(), ::tolower);

		/* Normalize to one slash type */
		std::ranges::replace(toReturn, '\\', '/');
	}

	return toReturn;
}

//Based off code from: https://www.liquisearch.com/fletchers_checksum/implementation/optimizations
uint32_t Zone::CalculateChecksum() const {
	uint32_t sum1 = 0xffff;
	uint32_t sum2 = 0xffff;

	for (const auto& [scene, sceneRevision] : m_MapRevisions) {
		uint32_t sceneID = scene.GetSceneID();
		sum2 += sum1 += (sceneID >> 16);
		sum2 += sum1 += (sceneID & 0xffff);

		uint32_t layerID = GeneralUtils::ToUnderlying(scene.GetLayerID());
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
		scene.level = std::make_unique<Level>(this, m_ZonePath + scene.filename, sceneID.GetSceneID());

		if (scene.level->m_ChunkHeaders.empty()) continue;

		scene.level->m_ChunkHeaders.begin()->second.lwoSceneID = sceneID;
		AddRevision(scene.level->m_ChunkHeaders.begin()->second.lwoSceneID, scene.level->m_ChunkHeaders.begin()->second.fileInfo.revision);
	}
}

bool Zone::LoadSceneMap() {
	if (m_ZoneRawPath.empty()) return false;
	auto file = Game::assetManager->GetFile((m_ZonePath + m_ZoneRawPath).c_str());
	if (!file) {
		LOG("Could not open the terrain file %s for its scene map", (m_ZonePath + m_ZoneRawPath).c_str());
		return false;
	}
	Raw::Raw raw;
	if (!Raw::ReadRaw(file, raw)) {
		LOG("Could not read the terrain file %s for its scene map", (m_ZonePath + m_ZoneRawPath).c_str());
		return false;
	}
	m_SceneMap = ZoneScenes::SceneMap(raw);
	return !m_SceneMap.Empty();
}

void Zone::AddRevision(LWOSCENEID sceneID, uint32_t revision) {
	if (m_MapRevisions.find(sceneID) == m_MapRevisions.end()) {
		m_MapRevisions.insert(std::make_pair(sceneID, revision));
	}
}

void Zone::LoadScene(ZoneScene&& zoneScene) {
	SceneRef scene;
	static_cast<ZoneScene&>(scene) = std::move(zoneScene);
	scene.level = nullptr;
	// Older files have no scene ID or layer: ZoneFile numbers their scenes, and the layer stays General
	LWOSCENEID lwoSceneID(scene.id, scene.sceneType);

	std::string luTriggersPath = scene.filename.substr(0, scene.filename.size() - 4) + ".lutriggers";
	if (Game::assetManager->HasFile((m_ZonePath + luTriggersPath).c_str())) LoadLUTriggers(luTriggersPath, scene);

	m_Scenes[lwoSceneID] = std::move(scene);
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

LUTriggers::Trigger* Zone::GetTrigger(uint32_t sceneID, uint32_t triggerID) const {
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
