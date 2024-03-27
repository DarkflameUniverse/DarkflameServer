#pragma once

#include "dZMCommon.h"
#include "LDFFormat.h"
#include "tinyxml2.h"
#include <string>
#include <vector>
#include <map>

namespace LUTriggers {
	struct Trigger;
};

class Level;

enum class eWaypointCommandType : uint32_t;

struct WaypointCommand {
	eWaypointCommandType command{};
	std::string data;
};


struct SceneRef {
	std::string filename;
	uint32_t id{};
	uint32_t sceneType{}; //0 = general, 1 = audio?
	std::string name;
	NiPoint3 unknown1;
	float unknown2{};
	uint8_t color_r{};
	uint8_t color_g{};
	uint8_t color_b{};
	Level* level;
	std::map<uint32_t, LUTriggers::Trigger*> triggers;
};

struct SceneTransitionInfo {
	uint64_t sceneID{}; //id of the scene being transitioned to.
	NiPoint3 position;
};

struct SceneTransition {
	std::string name;
	std::vector<SceneTransitionInfo> points;
	float width{};
};

struct MovingPlatformPathWaypoint {
	uint8_t lockPlayer{};
	float wait{};
	std::string departSound;
	std::string arriveSound;
};

struct CameraPathWaypoint {
	float time{};
	float fov{};
	float tension{};
	float continuity{};
	float bias{};
};

struct RacingPathWaypoint {
	uint8_t isResetNode{};
	uint8_t isNonHorizontalCamera{};
	float planeWidth{};
	float planeHeight{};
	float shortestDistanceToEnd{};
};

struct PathWaypoint {
	NiPoint3 position;
	NiQuaternion rotation; // not included in all, but it's more convenient here
	MovingPlatformPathWaypoint movingPlatform;
	CameraPathWaypoint camera;
	RacingPathWaypoint racing;
	float speed{};
	std::vector<LDFBaseData*> config;
	std::vector<WaypointCommand> commands;
};

enum class PathType : uint32_t {
	Movement = 0,
	MovingPlatform = 1,
	Property = 2,
	Camera = 3,
	Spawner = 4,
	Showcase = 5,
	Race = 6,
	Rail = 7
};

enum class PathBehavior : uint32_t {
	Loop = 0,
	Bounce = 1,
	Once = 2
};

enum class PropertyPathType : int32_t {
	Path = 0,
	EntireZone = 1,
	GenetatedRectangle = 2
};

enum class PropertyType : int32_t {
	Premiere = 0,
	Prize = 1,
	LUP = 2,
	Headspace = 3
};

enum class PropertyRentalPeriod : uint32_t {
	Forever = 0,
	Seconds = 1,
	Minutes = 2,
	Hours = 3,
	Days = 4,
	Weeks = 5,
	Months = 6,
	Years = 7
};

enum class PropertyAchievmentRequired : uint32_t {
	None = 0,
	Builder = 1,
	Craftsman = 2,
	SeniorBuilder = 3,
	JourneyMan = 4,
	MasterBuilder = 5,
	Architect = 6,
	SeniorArchitect = 7,
	MasterArchitect = 8,
	Visionary = 9,
	Exemplar = 10
};

struct MovingPlatformPath {
	std::string platformTravelSound;
	uint8_t timeBasedMovement{};
};

struct PropertyPath {
	PropertyPathType pathType{};
	int32_t price{};
	uint32_t rentalTime{};
	uint64_t associatedZone{};
	std::string displayName;
	std::string displayDesc;
	PropertyType type{};
	uint32_t cloneLimit{};
	float repMultiplier{};
	PropertyRentalPeriod rentalPeriod{};
	PropertyAchievmentRequired achievementRequired{};

	// Player respawn coordinates in the main zone (not the property zone)
	NiPoint3 playerZoneCoords;
	float maxBuildHeight{};
};

struct CameraPath {
	std::string nextPath;
	uint8_t rotatePlayer{};
};

struct SpawnerPath {
	LOT spawnedLOT{};
	uint32_t respawnTime{};
	int32_t maxToSpawn{};
	uint32_t amountMaintained{};
	LWOOBJID spawnerObjID;
	uint8_t spawnerNetActive{};
};


struct Path {
	uint32_t pathVersion{};
	PathType pathType;
	std::string pathName;
	uint32_t flags{};
	PathBehavior pathBehavior;
	uint32_t waypointCount{};
	std::vector<PathWaypoint> pathWaypoints;
	SpawnerPath spawner;
	MovingPlatformPath movingPlatform;
	PropertyPath property;
	CameraPath camera;
};

class Zone {
public:
	enum class FileFormatVersion : uint32_t { //Times are guessed.
		PrePreAlpha = 30,
		PreAlpha = 32,
		LatePreAlpha = 33,
		EarlyAlpha = 35,
		Alpha = 36,
		LateAlpha = 37,
		Beta = 38,
		Launch = 39,
		Auramar = 40,
		Latest = 41
	};

public:
	Zone(const LWOMAPID& mapID, const LWOINSTANCEID& instanceID, const LWOCLONEID& cloneID);
	~Zone();

	void Initalize();
	void LoadZoneIntoMemory();
	std::string GetFilePathForZoneID();
	uint32_t CalculateChecksum();
	void LoadLevelsIntoMemory();
	void AddRevision(LWOSCENEID sceneID, uint32_t revision);
	const LWOZONEID& GetZoneID() const { return m_ZoneID; }
	const uint32_t GetChecksum() const { return m_CheckSum; }
	LUTriggers::Trigger* GetTrigger(uint32_t sceneID, uint32_t triggerID);
	const Path* GetPath(std::string name) const;

	uint32_t GetWorldID() const { return m_WorldID; }
	[[nodiscard]] std::string GetZoneName() const { return m_ZoneName; }
	std::string GetZoneRawPath() const { return m_ZoneRawPath; }
	std::string GetZonePath() const { return m_ZonePath; }

	const NiPoint3& GetSpawnPos() const { return m_Spawnpoint; }
	const NiQuaternion& GetSpawnRot() const { return m_SpawnpointRotation; }

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
	NiQuaternion m_SpawnpointRotation;
	uint32_t m_SceneCount;

	std::string m_ZonePath; //Path to the .luz's folder
	std::string m_ZoneName; //Name given to the zone by a level designer
	std::string m_ZoneDesc; //Description of the zone by a level designer
	std::string m_ZoneRawPath; //Path to the .raw file of this zone.

	std::map<LWOSCENEID, SceneRef> m_Scenes;
	std::vector<SceneTransition> m_SceneTransitions;

	uint32_t m_PathDataLength;
	uint32_t m_PathChunkVersion;
	std::vector<Path> m_Paths;

	std::map<LWOSCENEID, uint32_t> m_MapRevisions; //rhs is the revision!
	//private ("helper") functions:
	void LoadScene(std::istream& file);
	void LoadLUTriggers(std::string triggerFile, SceneRef& scene);
	void LoadSceneTransition(std::istream& file);
	SceneTransitionInfo LoadSceneTransitionInfo(std::istream& file);
	void LoadPath(std::istream& file);
};
