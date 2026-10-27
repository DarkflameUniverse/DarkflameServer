#pragma once

#include <array>
#include <cstdint>
#include <iosfwd>
#include <map>
#include <string>
#include <vector>

#include "dCommonVars.h"
#include "NiColor.h"
#include "NiPoint3.h"
#include "NiQuaternion.h"
#include "LDFFormat.h"

// One of an object's render techniques (SceneLoader::ReadLvlObjectData, version 7 on)
struct LevelRenderTechnique {
	std::string name; // 64 bytes in the file
	uint32_t valueCount{};
	uint8_t isColor{};
	std::array<float, 16> values{};
};

struct SceneObject {
	LWOOBJID id{};
	LOT lot{};
	uint32_t nodeType = 1; // the client's default when the file has none, or one outside 0-10
	uint32_t glomId = 1;
	NiPoint3 position;
	NiQuaternion rotation = QuatUtils::IDENTITY;
	float scale = 1.0f;
	std::string renderTechniquesName; // 64 bytes in the file, only there when there are render techniques
	std::vector<LevelRenderTechnique> renderTechniques;
	LwoNameValue settings;
};

// Fog and object draw distances (SceneDrawDistances); -1 is the client's default for a value the file doesn't have
struct SceneDrawDistances {
	float fogNear = -1.0f;
	float fogFar = -1.0f;
	float postFogSolid = -1.0f;
	float postFogFade = -1.0f;
	float staticObjectDistance = -1.0f;
	float dynamicObjectDistance = -1.0f;
};

// The draw distances of a group of objects (version 40 on)
struct SceneCullSetting {
	uint32_t group{};
	float minDistance{};
	float maxDistance{};
};

// A scene's light environment (SceneLoader::ReadLightingInfo); the defaults are the client's for what older files don't have
struct LevelLighting {
	float blendTime = 10.0f; // version 45 on
	NiColor ambient;
	NiColor specular;
	NiColor upperHemi;
	NiPoint3 position;
	// Version 31 on; before 39 only the fog distances, the same in both
	SceneDrawDistances minDrawDistances;
	SceneDrawDistances maxDrawDistances;
	std::vector<SceneCullSetting> cullSettings; // version 40 on
	NiColor fogColor{ 0.75f, 0.88f, 0.91f }; // version 31 on
	NiColor directional; // version 36 on

	// Before version 42 the zone's spawn point is here. Before 33 the file has only a position, which the client
	// ignores: it uses a fixed spawn point instead
	bool hasSpawnPoint = false;
	NiPoint3 filePosition;
	NiPoint3 spawnPosition;
	NiQuaternion spawnRotation = QuatUtils::IDENTITY;
};

// The skydome's file and five more (version 34 on) the client reads and doesn't use (SceneLoader::ReadSkydomeInfo)
struct LevelSkydome {
	std::string filename;
	std::array<std::string, 5> extraFilenames;
};

/**
 * A scene file (.lvl), read as the client reads it (SceneLoader::LoadLvlFile): its chunk headers, environment, every
 * object in it with the object's settings parsed. Only reads; which objects the world
 * spawns (feature gating, client-only objects, spawners) stays in dZoneManager, so the dashboard reads scenes with the
 * same code.
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

	// The environment chunk: where its parts start in the file (0 for none)
	struct EnviromentChunk {
		uint32_t lightingStart{};
		uint32_t skydomeStart{};
		uint32_t editorSettingsStart{};
	};

	std::map<uint32_t, ChunkHeader> chunkHeaders;

	// The file's version: from the file info chunk, or the header of a file from before chunks
	uint32_t version{};
	uint32_t revision{};
	// Before chunks, from version 32: set when the file has no editor settings
	uint8_t noEditorSettings{};
	EnviromentChunk enviroment;
	bool hasLighting = false;
	LevelLighting lighting;
	LevelSkydome skydome;
	// Version 37 on: the editor's saved colours (the client keeps at most 32)
	std::vector<NiColor> editorColors;
	std::vector<SceneObject> objects;

	/**
	 * Reads the whole file. Throws std::runtime_error (BinaryIO) if it ends early; what was read before that stays
	 * (an object is only added once all of it was read).
	 */
	void Read(std::istream& file);

private:
	static ChunkHeader ReadChunkHeader(std::istream& file, uint32_t start);
	void ReadFileInfoChunk(std::istream& file, ChunkHeader& header);
	void ReadLightingInfo(std::istream& file);
	void ReadSkydomeInfo(std::istream& file);
	void ReadEditorSettings(std::istream& file);
	void ReadSceneObjectDataChunk(std::istream& file);
};
