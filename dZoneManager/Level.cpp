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
#include "dConfig.h"

Level::Level(Zone* parentZone, const std::string& filepath) {
	m_ParentZone = parentZone;

	auto stream = Game::assetManager->GetFile(filepath.c_str());

	if (!stream) {
		LOG("Failed to load %s", filepath.c_str());
		return;
	}
	
	ReadChunks(stream);
}

void Level::MakeSpawner(SceneObject obj) {
	SpawnerInfo spawnInfo = SpawnerInfo();
	SpawnerNode* node = new SpawnerNode();
	spawnInfo.templateID = obj.lot;
	spawnInfo.spawnerID = obj.id;
	spawnInfo.templateScale = obj.scale;
	node->position = obj.position;
	node->rotation = obj.rotation;
	node->config = obj.settings;
	spawnInfo.nodes.push_back(node);
	for (LDFBaseData* data : obj.settings) {
		if (!data) continue;
		if (data->GetKey() == u"spawntemplate") {
			spawnInfo.templateID = std::stoi(data->GetValueAsString());
		}

		if (data->GetKey() == u"spawner_node_id") {
			node->nodeID = std::stoi(data->GetValueAsString());
		}

		if (data->GetKey() == u"spawner_name") {
			spawnInfo.name = data->GetValueAsString();
		}

		if (data->GetKey() == u"max_to_spawn") {
			spawnInfo.maxToSpawn = std::stoi(data->GetValueAsString());
		}

		if (data->GetKey() == u"spawner_active_on_load") {
			spawnInfo.activeOnLoad = std::stoi(data->GetValueAsString());
		}

		if (data->GetKey() == u"active_on_load") {
			spawnInfo.activeOnLoad = std::stoi(data->GetValueAsString());
		}

		if (data->GetKey() == u"respawn") {
			if (data->GetValueType() == eLDFType::LDF_TYPE_FLOAT) // Floats are in seconds
			{
				spawnInfo.respawnTime = std::stof(data->GetValueAsString());
			} else if (data->GetValueType() == eLDFType::LDF_TYPE_U32) // Ints are in ms?
			{
				spawnInfo.respawnTime = std::stoul(data->GetValueAsString()) / 1000;
			}
		}
		if (data->GetKey() == u"spawnsGroupOnSmash") {
			spawnInfo.spawnsOnSmash = std::stoi(data->GetValueAsString());
		}
		if (data->GetKey() == u"spawnNetNameForSpawnGroupOnSmash") {
			spawnInfo.spawnOnSmashGroupName = data->GetValueAsString();
		}
		if (data->GetKey() == u"groupID") { // Load object groups
			std::string groupStr = data->GetValueAsString();
			spawnInfo.groups = GeneralUtils::SplitString(groupStr, ';');
			spawnInfo.groups.erase(spawnInfo.groups.end() - 1);
		}
		if (data->GetKey() == u"no_auto_spawn") {
			spawnInfo.noAutoSpawn = static_cast<LDFData<bool>*>(data)->GetValue();
		}
		if (data->GetKey() == u"no_timed_spawn") {
			spawnInfo.noTimedSpawn = static_cast<LDFData<bool>*>(data)->GetValue();
		}
		if (data->GetKey() == u"spawnActivator") {
			spawnInfo.spawnActivator = static_cast<LDFData<bool>*>(data)->GetValue();
		}
	}

	Game::zoneManager->MakeSpawner(spawnInfo);
}

void Level::ReadChunks(std::istream& file) {
	const uint32_t CHNK_HEADER = ('C' + ('H' << 8) + ('N' << 16) + ('K' << 24));

	while (!file.eof()) {
		uint32_t initPos = uint32_t(file.tellg());
		uint32_t header = 0;
		BinaryIO::BinaryRead(file, header);
		if (header == CHNK_HEADER) { //Make sure we're reading a valid CHNK
			Header header;
			BinaryIO::BinaryRead(file, header.id);
			BinaryIO::BinaryRead(file, header.chunkVersion);
			BinaryIO::BinaryRead(file, header.chunkType);
			BinaryIO::BinaryRead(file, header.size);
			BinaryIO::BinaryRead(file, header.startPosition);

			uint32_t target = initPos + header.size;
			file.seekg(header.startPosition);

			//We're currently not loading env or particle data
			if (header.id == ChunkTypeID::FileInfo) {
				ReadFileInfoChunk(file, header);
			} else if (header.id == ChunkTypeID::SceneObjectData) {
				ReadSceneObjectDataChunk(file, header);
			}

			m_ChunkHeaders.insert(std::make_pair(header.id, header));
			file.seekg(target);
		} else {
			if (initPos == std::streamoff(0)) { //Really old chunk version
				file.seekg(0);
				Header header;
				header.id = ChunkTypeID::FileInfo; //I guess?
				BinaryIO::BinaryRead(file, header.chunkVersion);
				BinaryIO::BinaryRead(file, header.chunkType);
				file.ignore(1);
				BinaryIO::BinaryRead(file, header.fileInfo.revision);

				if (header.chunkVersion >= 45) file.ignore(4);
				file.ignore(4 * (4 * 3));

				if (header.chunkVersion >= 31) {
					if (header.chunkVersion >= 39) {
						file.ignore(12 * 4);

						if (header.chunkVersion >= 40) {
							uint32_t s = 0;
							BinaryIO::BinaryRead(file, s);
							for (uint32_t i = 0; i < s; ++i) {
								file.ignore(4 * 3); //a uint and two floats
							}
						}
					} else {
						file.ignore(8);
					}

					file.ignore(3 * 4);
				}

				if (header.chunkVersion >= 36) {
					file.ignore(3 * 4);
				}

				if (header.chunkVersion < 42) {
					file.ignore(3 * 4);

					if (header.chunkVersion >= 33) {
						file.ignore(4 * 4);
					}
				}

				for (uint32_t i = 0; i < 6; ++i) {
					uint32_t count = 0;
					BinaryIO::BinaryRead(file, count);
					file.ignore(count);
				}

				file.ignore(4);

				uint32_t count = 0;
				BinaryIO::BinaryRead(file, count);
				file.ignore(count * 12);

				m_ChunkHeaders.insert(std::make_pair(header.id, header));

				//Now pretend to be a normal file and read Objects chunk:
				Header hdr;
				hdr.id = ChunkTypeID::SceneObjectData;
				ReadSceneObjectDataChunk(file, hdr);
				m_ChunkHeaders.insert(std::make_pair(hdr.id, hdr));
			} break;
		}
	}
}

void Level::ReadFileInfoChunk(std::istream& file, Header& header) {
	BinaryIO::BinaryRead(file, header.fileInfo.version);
	BinaryIO::BinaryRead(file, header.fileInfo.revision);
	BinaryIO::BinaryRead(file, header.fileInfo.enviromentChunkStart);
	BinaryIO::BinaryRead(file, header.fileInfo.objectChunkStart);
	BinaryIO::BinaryRead(file, header.fileInfo.particleChunkStart);

	//PATCH FOR AG: (messed up file?)
	if (header.fileInfo.revision == 0xCDCDCDCD && m_ParentZone->GetZoneID().GetMapID() == 1100) header.fileInfo.revision = 26;
}

void Level::ReadSceneObjectDataChunk(std::istream& file, Header& header) {
	uint32_t objectsCount = 0;
	BinaryIO::BinaryRead(file, objectsCount);

	CDFeatureGatingTable* featureGatingTable = CDClientManager::Instance().GetTable<CDFeatureGatingTable>();

	CDFeatureGating gating;
	gating.major = 1;
	gating.current = 10;
	gating.minor = 64;
	GeneralUtils::TryParse<int32_t>(Game::config->GetValue("version_major"), gating.major);
	GeneralUtils::TryParse<int32_t>(Game::config->GetValue("version_current"), gating.current);
	GeneralUtils::TryParse<int32_t>(Game::config->GetValue("version_minor"), gating.minor);

	const auto zoneControlObject = Game::zoneManager->GetZoneControlObject();
	DluAssert(zoneControlObject != nullptr);
	for (uint32_t i = 0; i < objectsCount; ++i) {
		std::u16string ldfString;
		SceneObject obj;
		BinaryIO::BinaryRead(file, obj.id);
		BinaryIO::BinaryRead(file, obj.lot);

		/*if (header.fileInfo.version >= 0x26)*/ BinaryIO::BinaryRead(file, obj.nodeType);
		/*if (header.fileInfo.version >= 0x20)*/ BinaryIO::BinaryRead(file, obj.glomId);

		BinaryIO::BinaryRead(file, obj.position);
		BinaryIO::BinaryRead(file, obj.rotation);
		BinaryIO::BinaryRead(file, obj.scale);
		BinaryIO::ReadString<uint32_t>(file, ldfString);
		BinaryIO::BinaryRead(file, obj.value3);

		//This is a little bit of a bodge, but because the alpha client (HF) doesn't store the
		//spawn position / rotation like the later versions do, we need to check the LOT for the spawn pos & set it.
		if (obj.lot == LOT_MARKER_PLAYER_START) {
			Game::zoneManager->GetZone()->SetSpawnPos(obj.position);
			Game::zoneManager->GetZone()->SetSpawnRot(obj.rotation);
		}

		std::string sData = GeneralUtils::UTF16ToWTF8(ldfString);
		std::stringstream ssData(sData);
		std::string token;
		char deliminator = '\n';

		while (std::getline(ssData, token, deliminator)) {
			LDFBaseData* ldfData = LDFBaseData::DataFromString(token);
			obj.settings.push_back(ldfData);
		}


		// We should never have more than 1 zone control object
		bool skipLoadingObject = obj.lot == zoneControlObject->GetLOT();
		for (LDFBaseData* data : obj.settings) {
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
				skipLoadingObject = static_cast<bool>(std::stoi(data->GetValueAsString()));
				break;
			}
		}

		if (skipLoadingObject) {
			for (auto* setting : obj.settings) {
				delete setting;
				setting = nullptr;
			}

			continue;
		}

		if (obj.lot == 176) { //Spawner
			MakeSpawner(obj);
		} else { //Regular object
			EntityInfo info;
			info.spawnerID = 0;
			info.id = obj.id;
			info.lot = obj.lot;
			info.pos = obj.position;
			info.rot = obj.rotation;
			info.settings = obj.settings;
			info.scale = obj.scale;
			Game::entityManager->CreateEntity(info);
		}
	}
}
