#pragma once
#include "dZMCommon.h"
#include <map>
#include <iostream>
#include "Zone.h"
#include "LevelFile.h"

class Level {
public:
	using ChunkTypeID = LevelFile::ChunkTypeID;
	using FileInfoChunk = LevelFile::FileInfoChunk;

	struct Header : LevelFile::ChunkHeader {
		LWOSCENEID lwoSceneID;
	};

public:
	// sceneID: the scene id of the zone scene the file is (EntityInfo::scene of its objects)
	Level(Zone* parentZone, const std::string& filepath, int32_t sceneID = -1);

	static void MakeSpawner(const SceneObject& obj, int32_t sceneID = -1);

	std::map<uint32_t, Header> m_ChunkHeaders;
private:
	Zone* m_ParentZone;
	int32_t m_SceneID = -1;

	//private functions:
	void LoadSceneObjects(const std::vector<SceneObject>& objects);
};
