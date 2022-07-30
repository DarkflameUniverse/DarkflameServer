#include "Game.h"
#include "Level.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include "BinaryIO.h"
#include "dLogger.h"
#include "Spawner.h"
#include "dZoneManager.h"
#include "GeneralUtils.h"
#include "Entity.h"
#include "EntityManager.h"
#include "CDFeatureGatingTable.h"
#include "CDClientManager.h"

Level::Level(Zone* parentZone, const std::string& filepath) {
	m_ParentZone = parentZone;
	std::ifstream file(filepath, std::ios_base::in | std::ios_base::binary);
	if (file) {
		ReadChunks(file);
	} else {
		Game::logger->Log("Level", "Failed to load %s", filepath.c_str());
	}

	file.close();
}

Level::~Level() {
	for (std::map<uint32_t, Header>::iterator it = m_ChunkHeaders.begin(); it != m_ChunkHeaders.end(); ++it) {
		if (it->second.id == Level::ChunkTypeID::FileInfo) delete it->second.fileInfo;
		if (it->second.id == Level::ChunkTypeID::SceneObjectData) delete it->second.sceneObjects;
	}
}

const void Level::PrintAllObjects() {
	for (std::map<uint32_t, Header>::iterator it = m_ChunkHeaders.begin(); it != m_ChunkHeaders.end(); ++it) {
		if (it->second.id == Level::ChunkTypeID::SceneObjectData) {
			it->second.sceneObjects->PrintAllObjects();
		}
	}
}

void Level::ReadChunks(std::ifstream& file) {
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
				FileInfoChunk* fileInfo = new FileInfoChunk();
				BinaryIO::BinaryRead(file, header.chunkVersion);
				BinaryIO::BinaryRead(file, header.chunkType);
				file.ignore(1);
				BinaryIO::BinaryRead(file, fileInfo->revision);

				if (header.chunkVersion >= 45) file.ignore(4);
				file.ignore(4 * (4 * 3));

				if (header.chunkVersion >= 31) {
					if (header.chunkVersion >= 39) {
						file.ignore(12 * 4);

						if (header.chunkVersion >= 40) {
							uint32_t s = 0;
							BinaryIO::BinaryRead(file, s);
							for (uint32_t i = 0; i < s; ++i) {
								file.ignore(4); //a uint
								file.ignore(4); //two floats
								file.ignore(4);
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

				header.fileInfo = fileInfo;
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

void Level::ReadFileInfoChunk(std::ifstream& file, Header& header) {
	FileInfoChunk* fi = new FileInfoChunk;
	BinaryIO::BinaryRead(file, fi->version);
	BinaryIO::BinaryRead(file, fi->revision);
	BinaryIO::BinaryRead(file, fi->enviromentChunkStart);
	BinaryIO::BinaryRead(file, fi->objectChunkStart);
	BinaryIO::BinaryRead(file, fi->particleChunkStart);
	header.fileInfo = fi;

	//PATCH FOR AG: (messed up file?)
	if (header.fileInfo->revision == 3452816845 && m_ParentZone->GetZoneID().GetMapID() == 1100) header.fileInfo->revision = 26;
}

void Level::ReadSceneObjectDataChunk(std::ifstream& file, Header& header) {
	SceneObjectDataChunk* chunk = new SceneObjectDataChunk;
	uint32_t objectsCount = 0;
	BinaryIO::BinaryRead(file, objectsCount);

	CDFeatureGatingTable* featureGatingTable = CDClientManager::Instance()->GetTable<CDFeatureGatingTable>("FeatureGating");

	for (uint32_t i = 0; i < objectsCount; ++i) {
		SceneObject obj;
		BinaryIO::BinaryRead(file, obj.id);
		BinaryIO::BinaryRead(file, obj.lot);

		/*if (header.fileInfo->version >= 0x26)*/ BinaryIO::BinaryRead(file, obj.value1);
		/*if (header.fileInfo->version >= 0x20)*/ BinaryIO::BinaryRead(file, obj.value2);

		BinaryIO::BinaryRead(file, obj.position);
		BinaryIO::BinaryRead(file, obj.rotation);
		BinaryIO::BinaryRead(file, obj.scale);

		//This is a little bit of a bodge, but because the alpha client (HF) doesn't store the
		//spawn position / rotation like the later versions do, we need to check the LOT for the spawn pos & set it.
		if (obj.lot == LOT_MARKER_PLAYER_START) {
			dZoneManager::Instance()->GetZone()->SetSpawnPos(obj.position);
			dZoneManager::Instance()->GetZone()->SetSpawnRot(obj.rotation);
		}

		std::u16string ldfString = u"";
		uint32_t length = 0;
		BinaryIO::BinaryRead(file, length);

		for (uint32_t i = 0; i < length; ++i) {
			uint16_t data;
			BinaryIO::BinaryRead(file, data);
			ldfString.push_back(data);
		}

		std::string sData = GeneralUtils::UTF16ToWTF8(ldfString);
		std::stringstream ssData(sData);
		std::string token;
		char deliminator = '\n';

		while (std::getline(ssData, token, deliminator)) {
			LDFBaseData* ldfData = LDFBaseData::DataFromString(token);
			obj.settings.push_back(ldfData);
		}

		BinaryIO::BinaryRead(file, obj.value3);

		// Feature gating
		bool gated = false;
		for (LDFBaseData* data : obj.settings) {
			if (data->GetKey() == u"gatingOnFeature") {
				std::string featureGate = data->GetValueAsString();

				if (!featureGatingTable->FeatureUnlocked(featureGate)) {
					gated = true;

					break;
				}
			}
		}

		if (gated) {
			for (auto* setting : obj.settings) {
				delete setting;
			}

			obj.settings.clear();

			continue;
		}

		if (obj.lot == 176) { //Spawner
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
				if (data) {
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
							spawnInfo.respawnTime = std::stoi(data->GetValueAsString()) / 1000;
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
			}
			Spawner* spawner = new Spawner(spawnInfo);
			dZoneManager::Instance()->AddSpawner(obj.id, spawner);
		} else { //Regular object
			EntityInfo info;
			info.spawnerID = 0;
			info.id = obj.id;
			info.lot = obj.lot;
			info.pos = obj.position;
			info.rot = obj.rotation;
			info.settings = obj.settings;
			info.scale = obj.scale;

			//Check to see if we shouldn't be loading this:
			bool clientOnly = false;
			bool serverOnly = false;
			std::string featureGate = "";
			for (LDFBaseData* data : obj.settings) {
				if (data) {
					if (data->GetKey() == u"loadOnClientOnly") {
						clientOnly = (bool)std::stoi(data->GetValueAsString());
						break;
					}
					if (data->GetKey() == u"loadSrvrOnly") {
						serverOnly = (bool)std::stoi(data->GetValueAsString());
						break;
					}
				}
			}

			if (!clientOnly) {

				// We should never have more than 1 zone control object
				const auto zoneControlObject = dZoneManager::Instance()->GetZoneControlObject();
				if (zoneControlObject != nullptr && info.lot == zoneControlObject->GetLOT())
					goto deleteSettings;

				EntityManager::Instance()->CreateEntity(info, nullptr);
			} else {
			deleteSettings:

				for (auto* setting : info.settings) {
					delete setting;
					setting = nullptr;
				}

				info.settings.clear();
				obj.settings.clear();
			}
		}
	}

	header.sceneObjects = chunk;
}
