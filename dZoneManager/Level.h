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

	struct SceneObjectDataChunk {
		std::map<LWOOBJID, SceneObject> objects;

		SceneObject& GetObject(LWOOBJID id) {
			for (std::map<LWOOBJID, SceneObject>::iterator it = objects.begin(); it != objects.end(); ++it) {
				if (it->first == id) return it->second;
			}
		}

		const void PrintAllObjects() {
			for (std::map<LWOOBJID, SceneObject>::iterator it = objects.begin(); it != objects.end(); ++it) {
				std::cout << "\t ID: " << it->first << " LOT: " << it->second.lot << std::endl;
			}
		}

		uint32_t GetObjectCount() { return objects.size(); }
	};

	struct Header {
		uint32_t id;
		uint16_t chunkVersion;
		ChunkTypeID chunkType;
		uint32_t size;
		uint32_t startPosition;
		FileInfoChunk* fileInfo;
		SceneObjectDataChunk* sceneObjects;
		LWOSCENEID lwoSceneID;
	};

public:
	Level(Zone* parentZone, const std::string& filepath);
	~Level();

	const void PrintAllObjects();

	std::map<uint32_t, Header> m_ChunkHeaders;
private:
	Zone* m_ParentZone;

	//private functions:
	void ReadChunks(std::ifstream& file);
	void ReadFileInfoChunk(std::ifstream& file, Header& header);
	void ReadSceneObjectDataChunk(std::ifstream& file, Header& header);
};
