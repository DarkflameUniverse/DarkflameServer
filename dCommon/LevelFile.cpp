#include "LevelFile.h"

#include <algorithm>
#include <istream>
#include <stdexcept>

#include "BinaryIO.h"
#include "GeneralUtils.h"
#include "Logger.h"

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
}

void LevelFile::Read(std::istream& file) {
	uint32_t magic = 0;
	BinaryIO::BinaryRead(file, magic);
	if (magic == CHNK_HEADER) {
		// SceneLoader::LoadLvlFile / LvlChunkFile::Read: the file info chunk starts the file and gives where the
		// environment, object and particle chunks start (0 for none)
		ChunkHeader fileInfo = ReadChunkHeader(file, 0);
		file.seekg(fileInfo.startPosition);
		ReadFileInfoChunk(file, fileInfo);
		chunkHeaders.insert({ fileInfo.id, fileInfo });
		version = fileInfo.fileInfo.version;
		revision = fileInfo.fileInfo.revision;

		if (fileInfo.fileInfo.enviromentChunkStart != 0) {
			const auto header = ReadChunkHeader(file, fileInfo.fileInfo.enviromentChunkStart);
			chunkHeaders.insert({ header.id, header });
			// EnvironmentChunk::ReadLvlChunk2000: where the lighting, skydome and editor settings start
			file.seekg(header.startPosition);
			BinaryIO::BinaryRead(file, enviroment.lightingStart);
			BinaryIO::BinaryRead(file, enviroment.skydomeStart);
			BinaryIO::BinaryRead(file, enviroment.editorSettingsStart);
			// The client reads all three only when there is lighting
			if (enviroment.lightingStart != 0) {
				file.seekg(enviroment.lightingStart);
				ReadLightingInfo(file);
				file.seekg(enviroment.skydomeStart);
				ReadSkydomeInfo(file);
				file.seekg(enviroment.editorSettingsStart);
				ReadEditorSettings(file);
			}
		}

		if (fileInfo.fileInfo.objectChunkStart != 0) {
			const auto header = ReadChunkHeader(file, fileInfo.fileInfo.objectChunkStart);
			chunkHeaders.insert({ header.id, header });
			file.seekg(header.startPosition);
			ReadSceneObjectDataChunk(file);
		}

		if (fileInfo.fileInfo.particleChunkStart != 0) {
			const auto header = ReadChunkHeader(file, fileInfo.fileInfo.particleChunkStart);
			chunkHeaders.insert({ header.id, header });
			file.seekg(header.startPosition);
			ReadParticles(file);
		}
		return;
	}

	// Scenes from before chunks: a header (SceneLoader::ReadLvlHeader), then each part in turn
	file.seekg(0);
	ChunkHeader header;
	header.id = ChunkTypeID::FileInfo;
	BinaryIO::BinaryRead(file, header.chunkVersion);
	BinaryIO::BinaryRead(file, header.chunkType);
	version = header.chunkVersion;
	if (version >= 32) BinaryIO::BinaryRead(file, noEditorSettings);
	if (version > 36) BinaryIO::BinaryRead(file, revision);
	header.fileInfo.version = version;
	header.fileInfo.revision = revision;
	chunkHeaders.insert({ header.id, header });

	// A newer file is refused ("newer than the client understands")
	if (version > 48) {
		LOG("Scene file version %u is newer than the client reads; nothing read from it", version);
		return;
	}

	ReadLightingInfo(file);
	ReadSkydomeInfo(file);
	ReadEditorSettings(file);
	ReadSceneObjectDataChunk(file);
	ReadSounds(file);
	ReadParticles(file);
}

LevelFile::ChunkHeader LevelFile::ReadChunkHeader(std::istream& file, uint32_t start) {
	// No check of the magic, as in the client (0x01063a00)
	file.seekg(start);
	uint32_t magic = 0;
	ChunkHeader header;
	BinaryIO::BinaryRead(file, magic);
	BinaryIO::BinaryRead(file, header.id);
	BinaryIO::BinaryRead(file, header.chunkVersion);
	BinaryIO::BinaryRead(file, header.chunkType);
	BinaryIO::BinaryRead(file, header.size);
	BinaryIO::BinaryRead(file, header.startPosition);
	return header;
}

void LevelFile::ReadFileInfoChunk(std::istream& file, ChunkHeader& header) {
	BinaryIO::BinaryRead(file, header.fileInfo.version);
	BinaryIO::BinaryRead(file, header.fileInfo.revision);
	BinaryIO::BinaryRead(file, header.fileInfo.enviromentChunkStart);
	BinaryIO::BinaryRead(file, header.fileInfo.objectChunkStart);
	BinaryIO::BinaryRead(file, header.fileInfo.particleChunkStart);
}

void LevelFile::ReadLightingInfo(std::istream& file) {
	// SceneLoader::ReadLightingInfo (0x0102f8f0)
	hasLighting = true;
	if (version >= 45) BinaryIO::BinaryRead(file, lighting.blendTime);
	BinaryIO::BinaryRead(file, lighting.ambient);
	BinaryIO::BinaryRead(file, lighting.specular);
	BinaryIO::BinaryRead(file, lighting.upperHemi);
	BinaryIO::BinaryRead(file, lighting.position);

	if (version >= 31) {
		if (version < 39) {
			BinaryIO::BinaryRead(file, lighting.maxDrawDistances.fogNear);
			BinaryIO::BinaryRead(file, lighting.maxDrawDistances.fogFar);
			lighting.minDrawDistances.fogNear = lighting.maxDrawDistances.fogNear;
			lighting.minDrawDistances.fogFar = lighting.maxDrawDistances.fogFar;
		} else {
			for (auto* distances : { &lighting.minDrawDistances, &lighting.maxDrawDistances }) {
				BinaryIO::BinaryRead(file, distances->fogNear);
				BinaryIO::BinaryRead(file, distances->fogFar);
				BinaryIO::BinaryRead(file, distances->postFogSolid);
				BinaryIO::BinaryRead(file, distances->postFogFade);
				BinaryIO::BinaryRead(file, distances->staticObjectDistance);
				BinaryIO::BinaryRead(file, distances->dynamicObjectDistance);
			}

			if (version >= 40) {
				uint32_t count = 0;
				BinaryIO::BinaryRead(file, count);
				for (uint32_t i = 0; i < count; ++i) {
					SceneCullSetting setting;
					BinaryIO::BinaryRead(file, setting.group);
					BinaryIO::BinaryRead(file, setting.minDistance);
					BinaryIO::BinaryRead(file, setting.maxDistance);
					lighting.cullSettings.push_back(setting);
				}
			}
		}
		BinaryIO::BinaryRead(file, lighting.fogColor);
	}

	if (version >= 36) BinaryIO::BinaryRead(file, lighting.directional);

	if (version < 42) {
		lighting.hasSpawnPoint = true;
		BinaryIO::BinaryRead(file, lighting.filePosition);
		if (version < 33) {
			lighting.spawnPosition = NiPoint3(347.0f, 284.0f, -325.0f);
			lighting.spawnRotation = NiQuaternion(0.97082251f, 0.0f, 0.23979919f, 0.0f);
		} else {
			lighting.spawnPosition = lighting.filePosition;
			BinaryIO::BinaryRead(file, lighting.spawnRotation);
		}
	}
}

void LevelFile::ReadSkydomeInfo(std::istream& file) {
	// SceneLoader::ReadSkydomeInfo (0x0102f550)
	BinaryIO::ReadString<uint32_t>(file, skydome.filename, BinaryIO::ReadType::String);
	if (version >= 34) {
		for (auto& filename : skydome.extraFilenames) BinaryIO::ReadString<uint32_t>(file, filename, BinaryIO::ReadType::String);
	}
}

void LevelFile::ReadEditorSettings(std::istream& file) {
	// SceneLoader::ReadEditorSettings (0x0101b090): their size, then a count of colours; the client keeps the first 32
	if (version < 37 || noEditorSettings != 0) return;
	uint32_t size = 0;
	BinaryIO::BinaryRead(file, size);
	uint32_t count = 0;
	BinaryIO::BinaryRead(file, count);
	for (uint32_t i = 0; i < count; ++i) {
		NiColor color;
		BinaryIO::BinaryRead(file, color);
		if (i < 32) editorColors.push_back(color);
	}
}

void LevelFile::ReadSceneObjectDataChunk(std::istream& file) {
	// SceneLoader::ReadLvlGameObjects: the client has no objects for a file older than version 3 ("Level file is
	// unsupported"), and before chunks reads nothing after them either
	if (version < 3) return;

	uint32_t objectsCount = 0;
	BinaryIO::BinaryRead(file, objectsCount);

	for (uint32_t i = 0; i < objectsCount; ++i) {
		// SceneLoader::ReadLvlObjectData (0x0103ba20)
		SceneObject obj;
		BinaryIO::BinaryRead(file, obj.id);
		BinaryIO::BinaryRead(file, obj.lot);

		if (version >= 38) {
			int32_t nodeType = 1;
			BinaryIO::BinaryRead(file, nodeType);
			if (nodeType > -1 && nodeType < 11) obj.nodeType = nodeType;
		}

		if (version >= 32) BinaryIO::BinaryRead(file, obj.glomId);

		BinaryIO::BinaryRead(file, obj.position);
		BinaryIO::BinaryRead(file, obj.rotation);
		BinaryIO::BinaryRead(file, obj.scale);
		const auto config = ReadWideString32(file);

		if (version >= 7) {
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

		for (const auto& token : GeneralUtils::SplitString(config, '\n')) obj.settings.ParseInsert(token);

		objects.push_back(std::move(obj));
	}
}

void LevelFile::ReadSounds(std::istream& file) {
	// SceneLoader::LoadSounds (0x0103c110), only in files from before chunks
	if (version < 5) return;
	uint32_t count = 0;
	BinaryIO::BinaryRead(file, count);
	for (uint32_t i = 0; i < count; ++i) {
		LevelSound sound;
		BinaryIO::BinaryRead(file, sound.position);
		if (version < 35) {
			// A 260 character name, up to its first null
			std::u16string name(260, u'\0');
			file.read(reinterpret_cast<char*>(name.data()), static_cast<std::streamsize>(name.size() * sizeof(char16_t)));
			name.resize(std::min(name.find(u'\0'), name.size()));
			sound.config.Insert<std::u16string>(u"soundName", name);
		} else {
			for (const auto& token : GeneralUtils::SplitString(ReadWideString32(file), '\n')) sound.config.ParseInsert(token);
		}
		if (file.fail()) throw std::runtime_error("Failed to read from istream.");
		sounds.push_back(std::move(sound));
	}
}

void LevelFile::ReadParticles(std::istream& file) {
	// SceneLoader::LoadParticles (0x01048320); before version 44 a file may end before them
	if (version < 5) return;
	if (version < 44 && file.peek() == std::char_traits<char>::eof()) {
		file.clear();
		return;
	}

	uint32_t count = 0;
	BinaryIO::BinaryRead(file, count);
	std::string config;
	for (uint32_t i = 0; i < count; ++i) {
		LevelParticle particle;
		if (version >= 43) BinaryIO::BinaryRead(file, particle.priority);
		BinaryIO::BinaryRead(file, particle.position);
		BinaryIO::BinaryRead(file, particle.rotation);
		particle.effectNames = ReadWideString32(file);
		if (version < 46) {
			// Before version 46 the effect names end with a wide null
			uint16_t nameTerminator{};
			BinaryIO::BinaryRead(file, nameTerminator);
		} else {
			config = ReadWideString32(file);
		}
		particle.config = config;
		if (file.fail()) throw std::runtime_error("Failed to read from istream.");
		// The client stops at a particle with no effect
		if (particle.effectNames.empty()) break;
		particles.push_back(std::move(particle));
	}
}
