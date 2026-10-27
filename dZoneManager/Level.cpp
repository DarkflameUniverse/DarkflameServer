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
#include "ClientVersion.h"
#include "dConfig.h"
#include <ranges>
#include <algorithm>
#include <stdexcept>

Level::Level(Zone* parentZone, const std::string& filepath) {
	m_ParentZone = parentZone;

	auto stream = Game::assetManager->GetFile(filepath.c_str());

	if (!stream) {
		LOG("Failed to load %s", filepath.c_str());
		return;
	}
	
	try {
		ReadChunks(stream);
	} catch (const std::runtime_error&) {
		// The client reads past the end of such a file; what was read before it is kept
		LOG("%s ends early; loading the %zu objects read before its end", filepath.c_str(), m_Objects.size());
	}
	LoadSceneObjects();
}

Level::Level(std::istream& file) {
	ReadChunks(file);
}

void Level::MakeSpawner(const SceneObject& obj) {
	SpawnerInfo spawnInfo = SpawnerInfo();
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

void Level::LoadSceneObjects() {
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
	for (const auto& obj : m_Objects) {
		//This is a little bit of a bodge, but because the alpha client (HF) doesn't store the
		//spawn position / rotation like the later versions do, we need to check the LOT for the spawn pos & set it.
		if (obj.lot == LOT_MARKER_PLAYER_START) {
			Game::zoneManager->GetZoneMut()->SetSpawnPos(obj.position);
			Game::zoneManager->GetZoneMut()->SetSpawnRot(obj.rotation);
		}

		// We should never have more than 1 zone control object
		bool skipLoadingObject = obj.lot == zoneControlObject->GetLOT();
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

namespace {
	constexpr uint32_t CHNK_HEADER = ('C' + ('H' << 8) + ('N' << 16) + ('K' << 24));

	// A fixed size, zero padded name
	std::string ReadName64(std::istream& file) {
		std::string name(64, '\0');
		file.read(name.data(), static_cast<std::streamsize>(name.size()));
		if (file.fail()) throw std::runtime_error("Failed to read from istream.");
		name.resize(std::min(name.find('\0'), name.size()));
		return name;
	}

	// A u32 count of wide characters
	std::string ReadWideString32(std::istream& file) {
		std::u16string value;
		BinaryIO::ReadString<uint32_t>(file, value);
		if (file.fail()) throw std::runtime_error("Failed to read from istream.");
		return GeneralUtils::UTF16ToWTF8(value);
	}

	// LwoNameValue::FromString (0x010fa220): entries are split at every comma and line break, empty ones and ones
	// without a '=' are left out
	void ParseConfig(LwoNameValue& config, const std::string& text) {
		size_t start = 0;
		while (start <= text.size()) {
			const auto end = std::min(text.find_first_of(",\n", start), text.size());
			const auto entry = text.substr(start, end - start);
			if (entry.find('=') != std::string::npos) config.ParseInsert(entry);
			start = end + 1;
		}
	}
}

void Level::ReadChunks(std::istream& file) {
	uint32_t magic = 0;
	BinaryIO::BinaryRead(file, magic);
	if (magic == CHNK_HEADER) {
		// SceneLoader::LoadLvlFile / LvlChunkFile::Read: the file info chunk starts the file and gives where the
		// environment, object and particle chunks start (0 for none)
		Header fileInfo = ReadChunkHeader(file, 0);
		file.seekg(fileInfo.startPosition);
		ReadFileInfoChunk(file, fileInfo);
		m_ChunkHeaders.insert({ fileInfo.id, fileInfo });
		m_Version = fileInfo.fileInfo.version;
		m_Revision = fileInfo.fileInfo.revision;

		if (fileInfo.fileInfo.enviromentChunkStart != 0) {
			const auto header = ReadChunkHeader(file, fileInfo.fileInfo.enviromentChunkStart);
			m_ChunkHeaders.insert({ header.id, header });
			// EnvironmentChunk::ReadLvlChunk2000: where the lighting, skydome and editor settings start
			file.seekg(header.startPosition);
			BinaryIO::BinaryRead(file, m_Enviroment.lightingStart);
			BinaryIO::BinaryRead(file, m_Enviroment.skydomeStart);
			BinaryIO::BinaryRead(file, m_Enviroment.editorSettingsStart);
			// The client reads all three only when there is lighting
			if (m_Enviroment.lightingStart != 0) {
				file.seekg(m_Enviroment.lightingStart);
				ReadLightingInfo(file);
				file.seekg(m_Enviroment.skydomeStart);
				ReadSkydomeInfo(file);
				file.seekg(m_Enviroment.editorSettingsStart);
				ReadEditorSettings(file);
			}
		}

		if (fileInfo.fileInfo.objectChunkStart != 0) {
			const auto header = ReadChunkHeader(file, fileInfo.fileInfo.objectChunkStart);
			m_ChunkHeaders.insert({ header.id, header });
			file.seekg(header.startPosition);
			ReadSceneObjectDataChunk(file);
		}

		if (fileInfo.fileInfo.particleChunkStart != 0) {
			const auto header = ReadChunkHeader(file, fileInfo.fileInfo.particleChunkStart);
			m_ChunkHeaders.insert({ header.id, header });
			file.seekg(header.startPosition);
			ReadParticles(file);
		}
		return;
	}

	// Scenes from before chunks: a header (SceneLoader::ReadLvlHeader), then each part in turn
	file.seekg(0);
	Header header;
	header.id = ChunkTypeID::FileInfo;
	BinaryIO::BinaryRead(file, header.chunkVersion);
	BinaryIO::BinaryRead(file, header.chunkType);
	m_Version = header.chunkVersion;
	if (m_Version >= 32) BinaryIO::BinaryRead(file, m_NoEditorSettings);
	if (m_Version > 36) BinaryIO::BinaryRead(file, m_Revision);
	header.fileInfo.version = m_Version;
	header.fileInfo.revision = m_Revision;
	m_ChunkHeaders.insert({ header.id, header });

	// A newer file is refused ("newer than the client understands")
	if (m_Version > 48) {
		LOG("Scene file version %u is newer than the client reads; nothing read from it", m_Version);
		return;
	}

	ReadLightingInfo(file);
	ReadSkydomeInfo(file);
	ReadEditorSettings(file);
	ReadSceneObjectDataChunk(file);
	ReadSounds(file);
	ReadParticles(file);
}

Level::Header Level::ReadChunkHeader(std::istream& file, uint32_t start) {
	// No check of the magic, as in the client (0x01063a00)
	file.seekg(start);
	uint32_t magic = 0;
	Header header;
	BinaryIO::BinaryRead(file, magic);
	BinaryIO::BinaryRead(file, header.id);
	BinaryIO::BinaryRead(file, header.chunkVersion);
	BinaryIO::BinaryRead(file, header.chunkType);
	BinaryIO::BinaryRead(file, header.size);
	BinaryIO::BinaryRead(file, header.startPosition);
	return header;
}

void Level::ReadFileInfoChunk(std::istream& file, Header& header) {
	BinaryIO::BinaryRead(file, header.fileInfo.version);
	BinaryIO::BinaryRead(file, header.fileInfo.revision);
	BinaryIO::BinaryRead(file, header.fileInfo.enviromentChunkStart);
	BinaryIO::BinaryRead(file, header.fileInfo.objectChunkStart);
	BinaryIO::BinaryRead(file, header.fileInfo.particleChunkStart);
}

void Level::ReadLightingInfo(std::istream& file) {
	// SceneLoader::ReadLightingInfo (0x0102f8f0)
	m_HasLighting = true;
	if (m_Version >= 45) BinaryIO::BinaryRead(file, m_Lighting.blendTime);
	BinaryIO::BinaryRead(file, m_Lighting.ambient);
	BinaryIO::BinaryRead(file, m_Lighting.specular);
	BinaryIO::BinaryRead(file, m_Lighting.upperHemi);
	BinaryIO::BinaryRead(file, m_Lighting.position);

	if (m_Version >= 31) {
		if (m_Version < 39) {
			BinaryIO::BinaryRead(file, m_Lighting.maxDrawDistances.fogNear);
			BinaryIO::BinaryRead(file, m_Lighting.maxDrawDistances.fogFar);
			m_Lighting.minDrawDistances.fogNear = m_Lighting.maxDrawDistances.fogNear;
			m_Lighting.minDrawDistances.fogFar = m_Lighting.maxDrawDistances.fogFar;
		} else {
			for (auto* distances : { &m_Lighting.minDrawDistances, &m_Lighting.maxDrawDistances }) {
				BinaryIO::BinaryRead(file, distances->fogNear);
				BinaryIO::BinaryRead(file, distances->fogFar);
				BinaryIO::BinaryRead(file, distances->postFogSolid);
				BinaryIO::BinaryRead(file, distances->postFogFade);
				BinaryIO::BinaryRead(file, distances->staticObjectDistance);
				BinaryIO::BinaryRead(file, distances->dynamicObjectDistance);
			}

			if (m_Version >= 40) {
				uint32_t count = 0;
				BinaryIO::BinaryRead(file, count);
				for (uint32_t i = 0; i < count; ++i) {
					SceneCullSetting setting;
					BinaryIO::BinaryRead(file, setting.group);
					BinaryIO::BinaryRead(file, setting.minDistance);
					BinaryIO::BinaryRead(file, setting.maxDistance);
					m_Lighting.cullSettings.push_back(setting);
				}
			}
		}
		BinaryIO::BinaryRead(file, m_Lighting.fogColor);
	}

	if (m_Version >= 36) BinaryIO::BinaryRead(file, m_Lighting.directional);

	if (m_Version < 42) {
		m_Lighting.hasSpawnPoint = true;
		BinaryIO::BinaryRead(file, m_Lighting.filePosition);
		if (m_Version < 33) {
			m_Lighting.spawnPosition = NiPoint3(347.0f, 284.0f, -325.0f);
			m_Lighting.spawnRotation = NiQuaternion(0.97082251f, 0.0f, 0.23979919f, 0.0f);
		} else {
			m_Lighting.spawnPosition = m_Lighting.filePosition;
			BinaryIO::BinaryRead(file, m_Lighting.spawnRotation);
		}
	}
}

void Level::ReadSkydomeInfo(std::istream& file) {
	// SceneLoader::ReadSkydomeInfo (0x0102f550)
	BinaryIO::ReadString<uint32_t>(file, m_Skydome.filename, BinaryIO::ReadType::String);
	if (m_Version >= 34) {
		for (auto& filename : m_Skydome.extraFilenames) BinaryIO::ReadString<uint32_t>(file, filename, BinaryIO::ReadType::String);
	}
}

void Level::ReadEditorSettings(std::istream& file) {
	// SceneLoader::ReadEditorSettings (0x0101b090): their size, then a count of colours; the client keeps the first 32
	if (m_Version < 37 || m_NoEditorSettings != 0) return;
	uint32_t size = 0;
	BinaryIO::BinaryRead(file, size);
	uint32_t count = 0;
	BinaryIO::BinaryRead(file, count);
	for (uint32_t i = 0; i < count; ++i) {
		NiColor color;
		BinaryIO::BinaryRead(file, color);
		if (i < 32) m_EditorColors.push_back(color);
	}
}

void Level::ReadSceneObjectDataChunk(std::istream& file) {
	// SceneLoader::ReadLvlGameObjects: the client has no objects for a file older than version 3 ("Level file is
	// unsupported"), and before chunks reads nothing after them either
	if (m_Version < 3) return;

	uint32_t objectsCount = 0;
	BinaryIO::BinaryRead(file, objectsCount);

	for (uint32_t i = 0; i < objectsCount; ++i) {
		// SceneLoader::ReadLvlObjectData (0x0103ba20)
		SceneObject obj;
		BinaryIO::BinaryRead(file, obj.id);
		BinaryIO::BinaryRead(file, obj.lot);

		if (m_Version >= 38) {
			int32_t nodeType = 1;
			BinaryIO::BinaryRead(file, nodeType);
			if (nodeType > -1 && nodeType < 11) obj.nodeType = nodeType;
		}

		if (m_Version >= 32) BinaryIO::BinaryRead(file, obj.glomId);

		BinaryIO::BinaryRead(file, obj.position);
		BinaryIO::BinaryRead(file, obj.rotation);
		BinaryIO::BinaryRead(file, obj.scale);
		const auto config = ReadWideString32(file);

		if (m_Version >= 7) {
			uint32_t techniqueCount = 0;
			BinaryIO::BinaryRead(file, techniqueCount);
			if (techniqueCount != 0) {
				obj.renderTechniquesName = ReadName64(file);
				obj.renderTechniques.resize(techniqueCount);
				for (auto& technique : obj.renderTechniques) {
					technique.name = ReadName64(file);
					BinaryIO::BinaryRead(file, technique.valueCount);
					BinaryIO::BinaryRead(file, technique.isColor);
					for (auto& value : technique.values) BinaryIO::BinaryRead(file, value);
				}
			}
		}
		if (file.fail()) throw std::runtime_error("Failed to read from istream.");

		ParseConfig(obj.settings, config);
		ApplyClientConfigFixups(obj);

		m_Objects.push_back(std::move(obj));
	}
}

void Level::ApplyClientConfigFixups(SceneObject& obj) const {
	// What ReadLvlObjectData changes in an object's config as it loads it
	auto& settings = obj.settings;
	const auto typeOf = [&settings](const std::u16string& key) {
		const auto it = settings.find(key);
		return it == settings.end() || !it->second ? LDF_TYPE_UNKNOWN : it->second->GetValueType();
	};
	const auto valueOf = [&settings]<typename T>(const std::u16string& key) {
		return static_cast<const LDFData<T>*>(settings.find(key)->second.get())->GetValue();
	};

	// Before version 47 a respawn time was in milliseconds: a float over 100, or any u32, becomes seconds (a float)
	if (m_Version < 47) {
		if (typeOf(u"respawn") == LDF_TYPE_FLOAT) {
			const auto respawn = valueOf.operator()<float>(u"respawn");
			if (respawn > 100.0f) settings.Insert<float>(u"respawn", respawn * 0.001f);
		} else if (typeOf(u"respawn") == LDF_TYPE_U32) {
			settings.Insert<float>(u"respawn", static_cast<float>(valueOf.operator()<uint32_t>(u"respawn") * 0.001));
		}
	}

	// A model spawner (LOT 176 spawning template 14): its blueprint is its subkey unless it has one, its behaviors are
	// off unless it says otherwise, and it never wraps its render
	if (obj.lot == 176 && typeOf(u"spawntemplate") == LDF_TYPE_S32 && valueOf.operator()<int32_t>(u"spawntemplate") == 14) {
		if (typeOf(u"blueprintid") == LDF_TYPE_UNKNOWN) {
			const LWOOBJID subkey = typeOf(u"subkey") == LDF_TYPE_OBJID ? valueOf.operator()<LWOOBJID>(u"subkey") : LWOOBJID_EMPTY;
			settings.Insert<LWOOBJID>(u"blueprintid", subkey);
		}
		if (typeOf(u"DisableModelBehaviors") == LDF_TYPE_UNKNOWN) settings.Insert<bool>(u"DisableModelBehaviors", true);
		settings.Insert<bool>(u"preventRenderWrapping", true);
	}

	// Before version 44 an object that overrides its scene goes to layer 0 of the scene it names
	if (m_Version < 44 && typeOf(u"sceneIDOverrideEnabled") != LDF_TYPE_UNKNOWN) settings.Insert<uint32_t>(u"sceneLayerIDOverride", 0);
}

void Level::ReadSounds(std::istream& file) {
	// SceneLoader::LoadSounds (0x0103c110), only in files from before chunks
	if (m_Version < 5) return;
	uint32_t count = 0;
	BinaryIO::BinaryRead(file, count);
	for (uint32_t i = 0; i < count; ++i) {
		LevelSound sound;
		BinaryIO::BinaryRead(file, sound.position);
		if (m_Version < 35) {
			// A 260 character name, up to its first null
			std::u16string name(260, u'\0');
			file.read(reinterpret_cast<char*>(name.data()), static_cast<std::streamsize>(name.size() * sizeof(char16_t)));
			name.resize(std::min(name.find(u'\0'), name.size()));
			sound.config.Insert<std::u16string>(u"soundName", name);
		} else {
			ParseConfig(sound.config, ReadWideString32(file));
		}
		if (file.fail()) throw std::runtime_error("Failed to read from istream.");
		m_Sounds.push_back(std::move(sound));
	}
}

void Level::ReadParticles(std::istream& file) {
	// SceneLoader::LoadParticles (0x01048320); before version 44 a file may end before them
	if (m_Version < 5) return;
	if (m_Version < 44 && file.peek() == std::char_traits<char>::eof()) {
		file.clear();
		return;
	}

	uint32_t count = 0;
	BinaryIO::BinaryRead(file, count);
	std::string config;
	for (uint32_t i = 0; i < count; ++i) {
		LevelParticle particle;
		if (m_Version >= 43) BinaryIO::BinaryRead(file, particle.priority);
		BinaryIO::BinaryRead(file, particle.position);
		BinaryIO::BinaryRead(file, particle.rotation);
		particle.effectNames = ReadWideString32(file);
		if (m_Version < 46) BinaryIO::BinaryRead(file, particle.unknown);
		else config = ReadWideString32(file);
		particle.config = config;
		if (file.fail()) throw std::runtime_error("Failed to read from istream.");
		// The client stops at a particle with no effect
		if (particle.effectNames.empty()) break;
		m_Particles.push_back(std::move(particle));
	}
}
