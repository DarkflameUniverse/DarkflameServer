#pragma once
#include "dZMCommon.h"
#include <map>
#include <iostream>
#include "Zone.h"

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
		uint32_t version;
		uint32_t revision;
		uint32_t enviromentChunkStart;
		uint32_t objectChunkStart;
		uint32_t particleChunkStart;
	};

	struct Header {
		uint32_t id;
		uint16_t chunkVersion;
		ChunkTypeID chunkType;
		uint32_t size;
		uint32_t startPosition;
		FileInfoChunk fileInfo;
		LWOSCENEID lwoSceneID;
	};

public:
	Level(Zone* parentZone, const std::string& filepath);
	
	static void MakeSpawner(SceneObject obj);

	std::map<uint32_t, Header> m_ChunkHeaders;
private:
	Zone* m_ParentZone;

	//private functions:
	void ReadChunks(std::istream& file);
	void ReadFileInfoChunk(std::istream& file, Header& header);
	void ReadSceneObjectDataChunk(std::istream& file, Header& header);
};
