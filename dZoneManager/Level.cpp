#include "Game.h"
#include "Level.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include "BinaryIO.h"
#include "Logger.h"
#include "Spawner.h"
#include "dZoneManager.h"
#include "GeneralUtils.h"
#include "Entity.h"
#include "EntityManager.h"
#include "CDFeatureGatingTable.h"
#include "CDClientManager.h"
#include "AssetManager.h"
#include "LevelFile.h"
#include "ZoneFileLog.h"
#include "ClientVersion.h"
#include "dConfig.h"
#include <ranges>

Level::Level(Zone* parentZone, const std::string& filepath, int32_t sceneID) {
	m_ParentZone = parentZone;
	m_SceneID = sceneID;

	auto stream = Game::assetManager->GetFile(filepath.c_str());

	if (!stream) {
		LOG("Failed to load %s", filepath.c_str());
		return;
	}
	ZoneFileLog::RecordAsset(Game::assetManager, ZoneFileLog::eKind::SCENE, filepath, stream);
	
	// The file itself is read by LevelFile (dCommon), which the dashboard uses too
	LevelFile levelFile;
	levelFile.Read(stream);
	for (const auto& [id, chunkHeader] : levelFile.chunkHeaders) {
		Header header;
		static_cast<LevelFile::ChunkHeader&>(header) = chunkHeader;
		m_ChunkHeaders.insert(std::make_pair(id, header));
	}
	LoadSceneObjects(levelFile.objects);
}

void Level::MakeSpawner(const SceneObject& obj, int32_t sceneID) {
	SpawnerInfo spawnInfo = SpawnerInfo();
	spawnInfo.scene = sceneID;
	SpawnerNode* node = new SpawnerNode();
	spawnInfo.templateID = obj.lot;
	spawnInfo.spawnerID = obj.id;
	spawnInfo.templateScale = obj.scale;
	node->position = obj.position;
	node->rotation = obj.rotation;
	node->config = obj.settings;
	spawnInfo.nodes.push_back(node);
	for (const auto& data : obj.settings.values | std::views::values) {
		if (!data) continue;
		if (data->GetKey() == u"spawntemplate") {
			spawnInfo.templateID = GeneralUtils::TryParse(data->GetValueAsString(), 0);
		}

		if (data->GetKey() == u"spawner_node_id") {
			node->nodeID = GeneralUtils::TryParse(data->GetValueAsString(), 0u);
		}

		if (data->GetKey() == u"spawner_name") {
			spawnInfo.name = data->GetValueAsString();
		}

		if (data->GetKey() == u"max_to_spawn") {
			spawnInfo.maxToSpawn = GeneralUtils::TryParse(data->GetValueAsString(), 0);
		}

		if (data->GetKey() == u"spawner_active_on_load") {
			spawnInfo.activeOnLoad = GeneralUtils::TryParse(data->GetValueAsString(), false);
		}

		if (data->GetKey() == u"active_on_load") {
			spawnInfo.activeOnLoad = GeneralUtils::TryParse(data->GetValueAsString(), false);
		}

		if (data->GetKey() == u"respawn") {
			if (data->GetValueType() == eLDFType::LDF_TYPE_FLOAT) // Floats are in seconds
			{
				spawnInfo.respawnTime = GeneralUtils::TryParse(data->GetValueAsString(), 0.0f);
			} else if (data->GetValueType() == eLDFType::LDF_TYPE_U32) // Ints are in ms
			{
				spawnInfo.respawnTime = GeneralUtils::TryParse(data->GetValueAsString(), 0) / 1000;
			}
		}
		if (data->GetKey() == u"spawnsGroupOnSmash") {
			spawnInfo.spawnsOnSmash = GeneralUtils::TryParse(data->GetValueAsString(), false);
		}
		if (data->GetKey() == u"spawnNetNameForSpawnGroupOnSmash") {
			spawnInfo.spawnOnSmashGroupName = data->GetValueAsString();
		}
		if (data->GetKey() == u"groupID") { // Load object groups
			spawnInfo.groups = GeneralUtils::SplitString(data->GetValueAsString(), ';');
			if (spawnInfo.groups.back().empty()) spawnInfo.groups.erase(spawnInfo.groups.end() - 1);
		}
		if (data->GetKey() == u"no_auto_spawn") {
			spawnInfo.noAutoSpawn = GeneralUtils::TryParse(data->GetValueAsString(), false);
		}
		if (data->GetKey() == u"no_timed_spawn") {
			spawnInfo.noTimedSpawn = GeneralUtils::TryParse(data->GetValueAsString(), false);
		}
		if (data->GetKey() == u"spawnActivator") {
			spawnInfo.spawnActivator = GeneralUtils::TryParse(data->GetValueAsString(), false);
		}
	}

	Game::zoneManager->MakeSpawner(spawnInfo);
}

void Level::LoadSceneObjects(const std::vector<SceneObject>& objects) {
	CDFeatureGatingTable* featureGatingTable = CDClientManager::GetTable<CDFeatureGatingTable>();

	CDFeatureGating gating;
	gating.major =
		GeneralUtils::TryParse<int32_t>(Game::config->GetValue("version_major")).value_or(ClientVersion::major);
	gating.current =
		GeneralUtils::TryParse<int32_t>(Game::config->GetValue("version_current")).value_or(ClientVersion::current);
	gating.minor =
		GeneralUtils::TryParse<int32_t>(Game::config->GetValue("version_minor")).value_or(ClientVersion::minor);

	const auto zoneControlObject = Game::zoneManager->GetZoneControlObject();
	DluAssert(zoneControlObject != nullptr);
	for (const auto& obj : objects) {
		//This is a little bit of a bodge, but because the alpha client (HF) doesn't store the
		//spawn position / rotation like the later versions do, we need to check the LOT for the spawn pos & set it.
		if (obj.lot == LOT_MARKER_PLAYER_START) {
			Game::zoneManager->GetZoneMut()->SetSpawnPos(obj.position);
			Game::zoneManager->GetZoneMut()->SetSpawnRot(obj.rotation);
		}

		// We should never have more than 1 zone control object; and the client never loads a scene object of LOT 1 (the
		// player; ReadLvlObjectData)
		bool skipLoadingObject = obj.lot == zoneControlObject->GetLOT() || obj.lot == 1;
		for (const auto& data : obj.settings | std::views::values) {
			if (!data) continue;
			if (data->GetKey() == u"gatingOnFeature") {
				gating.featureName = data->GetValueAsString();
				if (gating.featureName == Game::config->GetValue("event_1")) continue;
				else if (gating.featureName == Game::config->GetValue("event_2")) continue;
				else if (gating.featureName == Game::config->GetValue("event_3")) continue;
				else if (gating.featureName == Game::config->GetValue("event_4")) continue;
				else if (gating.featureName == Game::config->GetValue("event_5")) continue;
				else if (gating.featureName == Game::config->GetValue("event_6")) continue;
				else if (gating.featureName == Game::config->GetValue("event_7")) continue;
				else if (gating.featureName == Game::config->GetValue("event_8")) continue;
				else if (!featureGatingTable->FeatureUnlocked(gating)) {
					// The feature is not unlocked, so we can skip loading this object
					skipLoadingObject = true;
					break;
				}
			}
			// If this is a client only object, we can skip loading it
			if (data->GetKey() == u"loadOnClientOnly") {
				skipLoadingObject |= GeneralUtils::TryParse(data->GetValueAsString(), false);
				break;
			}
		}

		if (skipLoadingObject) {
			continue;
		}

		if (obj.lot == 176) { //Spawner
			MakeSpawner(obj, m_SceneID);
		} else { //Regular object
			EntityInfo info;
			info.spawnerID = 0;
			info.id = obj.id;
			info.lot = obj.lot;
			info.pos = obj.position;
			info.rot = obj.rotation;
			info.settings = obj.settings;
			info.scale = obj.scale;
			info.scene = m_SceneID;
			Game::entityManager->CreateEntity(info);
		}
	}
}
