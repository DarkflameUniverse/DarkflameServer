#include "LevelFile.h"

#include <istream>
#include <stdexcept>

#include "BinaryIO.h"
#include "GeneralUtils.h"

void LevelFile::Read(std::istream& file) {
	const uint32_t CHNK_HEADER = ('C' + ('H' << 8) + ('N' << 16) + ('K' << 24));

	while (!file.eof()) {
		uint32_t initPos = uint32_t(file.tellg());
		uint32_t header = 0;
		BinaryIO::BinaryRead(file, header);
		if (header == CHNK_HEADER) { //Make sure we're reading a valid CHNK
			ChunkHeader header;
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
				// The objects are laid out by the file's version, from its file info chunk
				const auto fileInfo = chunkHeaders.find(ChunkTypeID::FileInfo);
				ReadSceneObjectDataChunk(file, fileInfo == chunkHeaders.end() ? 0 : fileInfo->second.fileInfo.version);
			}

			chunkHeaders.insert(std::make_pair(header.id, header));
			file.seekg(target);
		} else {
			if (initPos == std::streamoff(0)) { //Really old chunk version
				file.seekg(0);
				ChunkHeader header;
				header.id = ChunkTypeID::FileInfo;
				BinaryIO::BinaryRead(file, header.chunkVersion);
				BinaryIO::BinaryRead(file, header.chunkType);
				uint8_t important = 0;
				BinaryIO::BinaryRead(file, important);
				// file.ignore(1); //probably used
				if (header.chunkVersion > 36) {
					BinaryIO::BinaryRead(file, header.fileInfo.revision);
				}
				// HARDCODED 3
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

				// skydome info
				uint32_t count = 0;
				BinaryIO::BinaryRead(file, count);
				file.ignore(count);

				if (header.chunkVersion >= 33) {
					for (uint32_t i = 0; i < 5; ++i) {
						uint32_t count = 0;
						BinaryIO::BinaryRead(file, count);
						file.ignore(count);
					}
				}
				// editor settings
				if (!important && header.chunkVersion >= 37){
					file.ignore(4);

					uint32_t count = 0;
					BinaryIO::BinaryRead(file, count);
					file.ignore(count * 12);

				}

				header.id = ChunkTypeID::SceneObjectData;
				header.fileInfo.version = header.chunkVersion;
				ReadSceneObjectDataChunk(file, header.fileInfo.version);
				chunkHeaders.insert(std::make_pair(header.id, header));
			} break;
		}
	}
}

void LevelFile::ReadFileInfoChunk(std::istream& file, ChunkHeader& header) {
	BinaryIO::BinaryRead(file, header.fileInfo.version);
	BinaryIO::BinaryRead(file, header.fileInfo.revision);
	BinaryIO::BinaryRead(file, header.fileInfo.enviromentChunkStart);
	BinaryIO::BinaryRead(file, header.fileInfo.objectChunkStart);
	BinaryIO::BinaryRead(file, header.fileInfo.particleChunkStart);
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
		BinaryIO::BinaryRead(file, obj.value3);
		if (file.fail()) throw std::runtime_error("Failed to read from istream.");

		for (const auto& token : GeneralUtils::SplitString(GeneralUtils::UTF16ToWTF8(ldfString), '\n')) {
			obj.settings.ParseInsert(token);
		}

		objects.push_back(std::move(obj));
	}
}
