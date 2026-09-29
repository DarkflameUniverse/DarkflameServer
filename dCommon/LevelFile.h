#pragma once

#include <cstdint>
#include <iosfwd>
#include <map>
#include <vector>

#include "dCommonVars.h"
#include "NiPoint3.h"
#include "NiQuaternion.h"
#include "LDFFormat.h"

struct SceneObject {
	LWOOBJID id;
	LOT lot;
	uint32_t nodeType = 1; // the client's default when the file has none, or one outside 0-10
	uint32_t glomId = 1;
	NiPoint3 position;
	NiQuaternion rotation = QuatUtils::IDENTITY;
	float scale = 1.0f;
	uint32_t renderTechniqueCount{};
	LwoNameValue settings;
};

/**
 * A scene file (.lvl) as the world server loads it (Level): its chunk headers and every object in it, with the object's
 * settings parsed. Only reads; which objects the world spawns (feature gating, client-only objects, spawners) stays in
 * dZoneManager, so the dashboard reads scenes with the same code.
 */
struct LevelFile {
	enum ChunkTypeID : uint16_t {
		FileInfo = 1000,
		SceneEnviroment = 2000,
		SceneObjectData,
		SceneParticleData
	};

	struct FileInfoChunk {
		uint32_t version{};
		uint32_t revision{};
		uint32_t enviromentChunkStart{};
		uint32_t objectChunkStart{};
		uint32_t particleChunkStart{};
	};

	struct ChunkHeader {
		uint32_t id{};
		uint16_t chunkVersion{};
		ChunkTypeID chunkType{};
		uint32_t size{};
		uint32_t startPosition{};
		FileInfoChunk fileInfo;
	};

	std::map<uint32_t, ChunkHeader> chunkHeaders;
	std::vector<SceneObject> objects;

	/**
	 * Reads the whole file. Throws std::runtime_error (BinaryIO) if it ends early; what was read before that stays
	 * (an object is only added once all of it was read).
	 */
	void Read(std::istream& file);

private:
	void ReadFileInfoChunk(std::istream& file, ChunkHeader& header);
	void ReadSceneObjectDataChunk(std::istream& file, uint32_t version);
};
