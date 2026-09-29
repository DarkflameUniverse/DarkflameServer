#include "LevelFile.h"

#include <algorithm>
#include <istream>
#include <stdexcept>

#include "BinaryIO.h"
#include "GeneralUtils.h"

void LevelFile::Read(std::istream& file) {
	const uint32_t CHNK_HEADER = ('C' + ('H' << 8) + ('N' << 16) + ('K' << 24));

	uint32_t magic = 0;
	BinaryIO::BinaryRead(file, magic);
	if (magic == CHNK_HEADER) {
		// SceneLoader::ReadLvlFile / DoLvlChunk: the file info chunk starts the file and gives where the environment,
		// object and particle chunks start (0 for none); only the objects are read here
		ChunkHeader fileInfo = ReadChunkHeader(file, 0);
		file.seekg(fileInfo.startPosition);
		ReadFileInfoChunk(file, fileInfo);
		chunkHeaders.insert({ fileInfo.id, fileInfo });
		for (const auto start : { fileInfo.fileInfo.enviromentChunkStart, fileInfo.fileInfo.objectChunkStart, fileInfo.fileInfo.particleChunkStart }) {
			if (start == 0) continue;
			const auto header = ReadChunkHeader(file, start);
			chunkHeaders.insert({ header.id, header });
			if (start != fileInfo.fileInfo.objectChunkStart) continue;
			// The objects are laid out by the file's version, from its file info chunk
			file.seekg(header.startPosition);
			ReadSceneObjectDataChunk(file, fileInfo.fileInfo.version);
		}
		return;
	}

	// Scenes from before chunks (SceneLoader::ReadLvlSections): a header, then lighting, skydome and editor settings
	// the world skips, then the objects
	file.seekg(0);
	ChunkHeader header;
	header.id = ChunkTypeID::FileInfo;
	BinaryIO::BinaryRead(file, header.chunkVersion);
	BinaryIO::BinaryRead(file, header.chunkType);
	// Only from version 32 (SceneLoader::ReadLvlHeader)
	uint8_t important = 0;
	if (header.chunkVersion >= 32) BinaryIO::BinaryRead(file, important);
	if (header.chunkVersion > 36) {
		BinaryIO::BinaryRead(file, header.fileInfo.revision);
	}
	// Lighting (SceneLoader::ReadLightingInfo)
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

	// skydome info; five more strings from version 34 (SceneLoader::ReadSkydomeInfo)
	uint32_t count = 0;
	BinaryIO::BinaryRead(file, count);
	file.ignore(count);

	if (header.chunkVersion >= 34) {
		for (uint32_t i = 0; i < 5; ++i) {
			uint32_t count = 0;
			BinaryIO::BinaryRead(file, count);
			file.ignore(count);
		}
	}
	// editor settings: their size and then that many bytes (SceneLoader::ReadEditorSettings)
	if (!important && header.chunkVersion >= 37) {
		uint32_t size = 0;
		BinaryIO::BinaryRead(file, size);
		file.ignore(size);
	}

	// The client has no objects for a file older than version 3 ("Level file is unsupported")
	header.fileInfo.version = header.chunkVersion;
	if (header.chunkVersion >= 3) {
		header.id = ChunkTypeID::SceneObjectData;
		ReadSceneObjectDataChunk(file, header.fileInfo.version);
	}
	chunkHeaders.insert(std::make_pair(header.id, header));
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

void LevelFile::ApplyClientConfigFixups(SceneObject& obj, uint32_t version) {
	// What ReadLvlObjectData (1.10.64, 0x0103ba20) changes in an object's config as it loads it
	auto& settings = obj.settings;
	const auto typeOf = [&settings](const std::u16string& key) {
		const auto it = settings.find(key);
		return it == settings.end() || !it->second ? LDF_TYPE_UNKNOWN : it->second->GetValueType();
	};
	const auto valueOf = [&settings]<typename T>(const std::u16string& key) {
		return static_cast<const LDFData<T>*>(settings.find(key)->second.get())->GetValue();
	};

	// Before version 47 a respawn time was in milliseconds: a float over 100, or any u32, becomes seconds (a float)
	if (version < 47) {
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
	if (version < 44 && typeOf(u"sceneIDOverrideEnabled") != LDF_TYPE_UNKNOWN) settings.Insert<uint32_t>(u"sceneLayerIDOverride", 0);
}

void LevelFile::ReadSceneObjectDataChunk(std::istream& file, uint32_t version) {
	uint32_t objectsCount = 0;
	BinaryIO::BinaryRead(file, objectsCount);

	for (uint32_t i = 0; i < objectsCount; ++i) {
		std::u16string ldfString;
		SceneObject obj;
		BinaryIO::BinaryRead(file, obj.id);
		BinaryIO::BinaryRead(file, obj.lot);

		if (version >= 38) {
			int32_t tmp = 1;
			BinaryIO::BinaryRead(file, tmp);
			if (tmp > -1 && tmp < 11) obj.nodeType = tmp;
		}

		if (version >= 32) {
			BinaryIO::BinaryRead(file, obj.glomId);
		}

		BinaryIO::BinaryRead(file, obj.position);
		BinaryIO::BinaryRead(file, obj.rotation);
		BinaryIO::BinaryRead(file, obj.scale);
		BinaryIO::ReadString<uint32_t>(file, ldfString);
		// From version 7 the object's render techniques: a count, and when there are any a 64 byte header and 133 bytes
		// each (64 byte name, u32, u8, 16 floats) (ReadLvlObjectData)
		if (version > 6) {
			BinaryIO::BinaryRead(file, obj.renderTechniqueCount);
			if (obj.renderTechniqueCount != 0) {
				std::string techniques(64 + static_cast<size_t>(obj.renderTechniqueCount) * (64 + 4 + 1 + 64), '\0');
				file.read(techniques.data(), static_cast<std::streamsize>(techniques.size()));
			}
		}
		if (file.fail()) throw std::runtime_error("Failed to read from istream.");

		// LDF_FROM_STRING (0x010fa220): entries are split at every comma and line break, empty ones and ones without a
		// '=' are left out
		const auto config = GeneralUtils::UTF16ToWTF8(ldfString);
		size_t start = 0;
		while (start <= config.size()) {
			const auto end = std::min(config.find_first_of(",\n", start), config.size());
			const auto entry = config.substr(start, end - start);
			if (entry.find('=') != std::string::npos) obj.settings.ParseInsert(entry);
			start = end + 1;
		}
		ApplyClientConfigFixups(obj, version);

		objects.push_back(std::move(obj));
	}
}
