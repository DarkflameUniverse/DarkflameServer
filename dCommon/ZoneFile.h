#pragma once

#include <cstdint>
#include <iosfwd>
#include <map>
#include <string>
#include <vector>

#include "dCommonVars.h"
#include "NiPoint3.h"
#include "NiQuaternion.h"
#include "LDFFormat.h"
#include "eWaypointCommandType.h"

/**
 * A zone file (.luz) as the world server loads it (Zone::LoadZoneIntoMemory): its scenes, scene transitions and paths.
 * Only reads; what the world does with the result (spawners, triggers, navmesh heights) stays in dZoneManager, so the
 * dashboard reads zones with the same code.
 */

struct WaypointCommand {
	eWaypointCommandType command{};
	std::string data;
};

struct ZoneScene {
	std::string filename;
	uint32_t id{};
	uint32_t sceneType{}; //0 = general, 1 = audio?
	std::string name;
	NiPoint3 unknown1;
	float unknown2{};
	uint8_t color_r{};
	uint8_t color_g{};
	uint8_t color_b{};
};

// A line the player crosses to leave the zone (the client's LuzReader::ReadZoneBoundaryLines)
struct ZoneBoundary {
	NiPoint3 normal;
	NiPoint3 point;
	LWOZONEID destZoneID; // clone 0
	uint32_t destSceneID{};
	NiPoint3 spawnLocation;
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
	NiQuaternion rotation = QuatUtils::IDENTITY; // not included in all, but it's more convenient here
	MovingPlatformPathWaypoint movingPlatform;
	CameraPathWaypoint camera;
	RacingPathWaypoint racing;
	float speed{};
	LwoNameValue config;
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

struct ZoneFile {
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

	FileFormatVersion fileFormatVersion{};
	uint32_t mapRevision{};
	uint32_t worldID{}; //should be equal to the MapID
	NiPoint3 spawnpoint;
	NiQuaternion spawnpointRotation = QuatUtils::IDENTITY;

	std::vector<ZoneScene> scenes;

	std::vector<ZoneBoundary> zoneBoundaries;

	std::string zoneRawPath; //Path to the .raw file of this zone.
	std::string zoneName; //Name given to the zone by a level designer
	std::string zoneDesc; //Description of the zone by a level designer

	std::vector<SceneTransition> sceneTransitions;

	uint32_t pathDataLength{};
	uint32_t pathChunkVersion{};
	std::vector<Path> paths;

	/**
	 * Reads the whole file. Throws std::runtime_error (BinaryIO) if it ends early; the last value read may still come up
	 * short without a throw, so callers that must know check the stream afterwards.
	 */
	void Read(std::istream& file);

	/**
	 * Reads only the start of the file: the version, spawn point, scenes and the terrain file's name, leaving the scene
	 * transitions and paths (most of the file in big zones) unread. Throws as Read does.
	 */
	void ReadHeader(std::istream& file);

private:
	void ReadScene(std::istream& file);
	void ReadZoneBoundaries(std::istream& file);
	void ReadSceneTransition(std::istream& file);
	SceneTransitionInfo ReadSceneTransitionInfo(std::istream& file);
	void ReadPath(std::istream& file);
	// A waypoint's name/value pairs: waypoint commands on movement and rail paths, LDF config on the others
	static void ReadLdfConfig(std::istream& file, PathType pathType, PathWaypoint& waypoint);
};
