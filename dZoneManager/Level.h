#pragma once
#include "dZMCommon.h"
#include <map>
#include <iostream>
#include "NiColor.h"
#include "Zone.h"

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

// A sound placed in the scene (SceneLoader::LoadSounds, version 5 on). Only the level editor makes anything of them
struct LevelSound {
	NiPoint3 position;
	LwoNameValue config; // before version 35 only a 260 character soundName
};

// A particle effect placed in the scene (SceneLoader::LoadParticles, version 5 on)
struct LevelParticle {
	int16_t priority{}; // version 43 on
	NiPoint3 position;
	NiQuaternion rotation = QuatUtils::IDENTITY;
	std::string effectNames; // separated by ';'
	uint16_t unknown{}; // before version 46
	// Version 46 on. The client keeps the last one it read for a particle without one
	std::string config;
};

// A scene file (.lvl), read as the client reads it (SceneLoader::LoadLvlFile)
class Level {
public:
	enum ChunkTypeID : uint16_t {
		FileInfo = 1000,
		SceneEnviroment = 2000,
		SceneObjectData,
		SceneParticleData
	};

	enum ChunkTypes {
		Enviroment,
		Objects,
		Particles
	};

	struct FileInfoChunk {
		uint32_t version{};
		uint32_t revision{};
		uint32_t enviromentChunkStart{};
		uint32_t objectChunkStart{};
		uint32_t particleChunkStart{};
	};

	struct Header {
		uint32_t id{};
		uint16_t chunkVersion{};
		ChunkTypeID chunkType{};
		uint32_t size{};
		uint32_t startPosition{};
		FileInfoChunk fileInfo;
		LWOSCENEID lwoSceneID;
	};

	// The environment chunk: where its parts start in the file (0 for none)
	struct EnviromentChunk {
		uint32_t lightingStart{};
		uint32_t skydomeStart{};
		uint32_t editorSettingsStart{};
	};

public:
	// Reads the scene file and loads its objects into the world
	Level(Zone* parentZone, const std::string& filepath);
	// Only reads the scene file. Throws std::runtime_error (BinaryIO) if it ends early; what was read before that
	// stays (an object is only added once all of it was read)
	explicit Level(std::istream& file);

	static void MakeSpawner(const SceneObject& obj);

	uint32_t GetVersion() const { return m_Version; }
	uint32_t GetRevision() const { return m_Revision; }
	const EnviromentChunk& GetEnviroment() const { return m_Enviroment; }
	bool HasLighting() const { return m_HasLighting; }
	const LevelLighting& GetLighting() const { return m_Lighting; }
	const LevelSkydome& GetSkydome() const { return m_Skydome; }
	const std::vector<NiColor>& GetEditorColors() const { return m_EditorColors; }
	const std::vector<SceneObject>& GetObjects() const { return m_Objects; }
	const std::vector<LevelSound>& GetSounds() const { return m_Sounds; }
	const std::vector<LevelParticle>& GetParticles() const { return m_Particles; }

	std::map<uint32_t, Header> m_ChunkHeaders;
private:
	Zone* m_ParentZone = nullptr;

	// The file's version: from the file info chunk, or the header of a file from before chunks
	uint32_t m_Version{};
	uint32_t m_Revision{};
	// Before chunks, from version 32: set when the file has no editor settings
	uint8_t m_NoEditorSettings{};
	EnviromentChunk m_Enviroment;
	bool m_HasLighting = false;
	LevelLighting m_Lighting;
	LevelSkydome m_Skydome;
	// Version 37 on: the editor's saved colours (the client keeps at most 32)
	std::vector<NiColor> m_EditorColors;
	std::vector<SceneObject> m_Objects;
	std::vector<LevelSound> m_Sounds;
	std::vector<LevelParticle> m_Particles;

	//private functions:
	void ReadChunks(std::istream& file);
	static Header ReadChunkHeader(std::istream& file, uint32_t start);
	void ReadFileInfoChunk(std::istream& file, Header& header);
	void ReadLightingInfo(std::istream& file);
	void ReadSkydomeInfo(std::istream& file);
	void ReadEditorSettings(std::istream& file);
	void ReadSceneObjectDataChunk(std::istream& file);
	void ReadSounds(std::istream& file);
	void ReadParticles(std::istream& file);
	void LoadSceneObjects();
};
