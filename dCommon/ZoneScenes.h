#pragma once

#include <cstdint>
#include <set>
#include <string>
#include <vector>

#include "Raw.h"
#include "ZoneFile.h"

/**
 * Which of a zone's scenes the game client keeps loaded, as it decides it (Zone::StreamScenesAroundPosition,
 * 0x0108a3f0 in client 1.10.64): the scene under the player, found in the terrain's scene map
 * (TerrainManager::GetSceneAtPos, 0x01069010), and the scenes connected to it by the zone file's scene transitions
 * (LevelManager's connected scenes, 0x01066500); the global scene (0) always stays. A scene is a scene id: its layers
 * (the audio scenes share their scene's id) come and go with it. Pure, so the world server and the dashboard use the
 * same rules and both can be unit tested.
 */
namespace ZoneScenes {
	constexpr uint32_t GLOBAL_SCENE = 0;
	// What the scene map holds where it names no scene; the client reads it as the global scene
	constexpr uint8_t NO_SCENE = 0xFF;

	// The terrain's scene map, kept apart from the rest of the .raw file (heights, textures and flairs aren't needed)
	class SceneMap {
	public:
		SceneMap() = default;
		explicit SceneMap(const Raw::Raw& raw);

		// The scene at world position (x, z), as the client finds it: the global scene outside the map or where it has none
		uint32_t SceneAt(float x, float z) const;
		bool Empty() const { return m_Chunks.empty(); }

	private:
		struct Chunk {
			float minX{}, minZ{}, maxX{}, maxZ{};
			float cellsPerUnitX{}, cellsPerUnitZ{}; // colorMapResolution / the chunk's width in world units
			uint32_t resolution{};
			std::vector<uint8_t> scenes; // resolution * resolution, x major
		};
		std::vector<Chunk> m_Chunks;
		float m_MinX{}, m_MinZ{}, m_MaxX{}, m_MaxZ{};
	};

	// The scenes each scene is connected to, from the zone file's transitions
	class SceneGraph {
	public:
		SceneGraph() = default;
		/**
		 * From a zone's scene transitions: each connects its first two points' scenes (the client's zone keeps them as
		 * scene pairs). Transitions naming a scene the zone doesn't have are dropped, as the client's
		 * Zone::FixupInvalidTransitions (0x010842e0) does.
		 */
		SceneGraph(const std::vector<ZoneScene>& scenes, const std::vector<SceneTransition>& transitions);

		// The scenes connected to `scene` (not itself); none for the global scene
		const std::set<uint32_t>& Neighbours(uint32_t scene) const;

		// What the client keeps loaded with the player in `scene`: the global scene, `scene` and its neighbours
		std::set<uint32_t> Loaded(uint32_t scene) const;

		const std::set<uint32_t>& Scenes() const { return m_Scenes; }

	private:
		std::set<uint32_t> m_Scenes;
		std::vector<std::pair<uint32_t, std::set<uint32_t>>> m_Neighbours;
	};

	/**
	 * The first `count` cells of a scene map as runs, for sending: a run is a byte with its length (1 to 255) then the
	 * scene id. Scene maps are large areas of one scene, so this is a small fraction of the cells.
	 */
	std::string RunLengths(const std::vector<uint8_t>& cells, size_t count);

	/**
	 * Whether a player with `loaded` scenes (SceneGraph::Loaded of the scene under them) gets an object: when its scene
	 * is loaded. An object's scene is the scene it was placed in (`placedScene`, -1 for none: spawned at run time or a
	 * player), else the scene under it (`sceneUnder`).
	 */
	inline bool InLoadedScene(int32_t placedScene, uint32_t sceneUnder, const std::set<uint32_t>& loaded) {
		return loaded.contains(placedScene >= 0 ? static_cast<uint32_t>(placedScene) : sceneUnder);
	}
}
