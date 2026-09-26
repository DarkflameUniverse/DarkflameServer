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
	Level(Zone* parentZone, const std::string& filepath);
	
	static void MakeSpawner(const SceneObject& obj);

	std::map<uint32_t, Header> m_ChunkHeaders;
private:
	Zone* m_ParentZone;

	//private functions:
	void LoadSceneObjects(const std::vector<SceneObject>& objects);
};
