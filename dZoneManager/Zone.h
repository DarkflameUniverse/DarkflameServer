#pragma once

#include "dZMCommon.h"
#include "LDFFormat.h"
#include "LWOSCENEID.h"
#include "ZoneFile.h"
#include "ZoneScenes.h"
#include "tinyxml2.h"
#include <string>
#include <vector>
#include <map>

namespace LUTriggers {
	struct Trigger;
};

class Level;

// A scene of the zone, with its level once loaded
struct SceneRef : ZoneScene {
	std::unique_ptr<Level> level;
	std::map<uint32_t, LUTriggers::Trigger*> triggers;
};

class Zone {
public:
	using FileFormatVersion = ZoneFile::FileFormatVersion;

public:
	Zone(const LWOZONEID zoneID);
	~Zone();

	void Initalize();
	void LoadZoneIntoMemory();
	std::string GetFilePathForZoneID() const;
	uint32_t CalculateChecksum() const;
	void LoadLevelsIntoMemory();
	void AddRevision(LWOSCENEID sceneID, uint32_t revision);
	const LWOZONEID& GetZoneID() const { return m_ZoneID; }
	const uint32_t GetChecksum() const { return m_CheckSum; }
	LUTriggers::Trigger* GetTrigger(uint32_t sceneID, uint32_t triggerID) const;
	const Path* GetPath(std::string name) const;
	const std::vector<Path>& GetPaths() const { return m_Paths; }

	uint32_t GetWorldID() const { return m_WorldID; }
	[[nodiscard]] std::string GetZoneName() const { return m_ZoneName; }
	std::string GetZoneRawPath() const { return m_ZoneRawPath; }
	std::string GetZonePath() const { return m_ZonePath; }

	const NiPoint3& GetSpawnPos() const { return m_Spawnpoint; }
	const NiQuaternion& GetSpawnRot() const { return m_SpawnpointRotation; }

	// Which scenes the client keeps loaded where (ZoneScenes); the scene map is empty unless LoadSceneMap read it
	const ZoneScenes::SceneGraph& GetSceneGraph() const { return m_SceneGraph; }
	const ZoneScenes::SceneMap& GetSceneMap() const { return m_SceneMap; }
	// Reads the terrain file's scene map (for scene ghosting); false when the zone has none
	bool LoadSceneMap();

	void SetSpawnPos(const NiPoint3& pos) { m_Spawnpoint = pos; }
	void SetSpawnRot(const NiQuaternion& rot) { m_SpawnpointRotation = rot; }

private:
	LWOZONEID m_ZoneID;
	std::string m_ZoneFilePath;
	uint32_t m_NumberOfObjectsLoaded;
	uint32_t m_NumberOfSceneTransitionsLoaded;
	FileFormatVersion m_FileFormatVersion;
	uint32_t m_CheckSum;
	uint32_t m_WorldID; //should be equal to the MapID
	NiPoint3 m_Spawnpoint;
	NiQuaternion m_SpawnpointRotation = QuatUtils::IDENTITY;
	uint32_t m_SceneCount;

	std::string m_ZonePath; //Path to the .luz's folder
	std::string m_ZoneName; //Name given to the zone by a level designer
	std::string m_ZoneDesc; //Description of the zone by a level designer
	std::string m_ZoneRawPath; //Path to the .raw file of this zone.

	std::map<LWOSCENEID, SceneRef> m_Scenes;
	std::vector<SceneTransition> m_SceneTransitions;
	ZoneScenes::SceneGraph m_SceneGraph;
	ZoneScenes::SceneMap m_SceneMap;

	uint32_t m_PathDataLength;
	uint32_t m_PathChunkVersion;
	std::vector<Path> m_Paths;

	std::map<LWOSCENEID, uint32_t> m_MapRevisions; //rhs is the revision!
	//private ("helper") functions:
	void LoadScene(ZoneScene&& zoneScene);
	void LoadLUTriggers(std::string triggerFile, SceneRef& scene);
};
