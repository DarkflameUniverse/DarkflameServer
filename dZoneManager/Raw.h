#pragma once

#ifndef __RAW_H__
#define __RAW_H__

#include <cstdint>
#include <optional>
#include <vector>
#include <string>
#include <istream>
#include "NiPoint3.h"
#include "dCommonVars.h"

namespace Raw {

/**
 * @brief Flair attributes structure
 * Represents decorative elements on the terrain
 */
struct FlairAttributes {
	uint32_t id;
	float scaleFactor;
	NiPoint3 position;
	NiPoint3 rotation;
	uint8_t colorR;
	uint8_t colorG;
	uint8_t colorB;
	// Padding: never set when a flair is placed, so it holds leftover memory; nothing reads it
	uint8_t padding;
};
static_assert(sizeof(FlairAttributes) == 36, "a flair record in the .raw is 36 bytes");

/**
 * @brief Mesh triangle structure
 * Contains triangle indices for terrain mesh
 */
struct MeshTri {
	uint16_t meshTriListSize;
	std::vector<uint16_t> meshTriList;
};

/**
 * @brief Vertex with scene ID
 * Used for the generated terrain mesh to enable fast scene lookups
 */
struct SceneVertex {
	NiPoint3 position;
	uint8_t sceneID;
	
	SceneVertex() : position(), sceneID(0) {}
	SceneVertex(const NiPoint3& pos, uint8_t scene) : position(pos), sceneID(scene) {}
};

/**
 * @brief Generated terrain mesh
 * Contains vertices with scene IDs for fast scene lookups at arbitrary positions
 */
struct TerrainMesh {
	std::vector<SceneVertex> vertices;
	std::vector<uint32_t> triangles; // Indices into vertices array (groups of 3)
	
	TerrainMesh() = default;
};

/**
 * @brief Terrain chunk structure
 * Represents a single chunk of terrain with heightmap, textures, flair, scene map and meshes
 */
struct Chunk {
	uint32_t id;
	uint32_t width;
	uint32_t height;
	float offsetX;
	float offsetZ;
	uint32_t shaderId = 0; // version < 32 only

	// Texture IDs (4 textures per chunk)
	std::vector<uint32_t> textureIds;

	// Terrain scale factor (world units between two heightmap points)
	float scaleFactor;

	// Heightmap data (width * height floats)
	std::vector<float> heightMap;

	// Color map: colorMapResolution^2 RGBA pixels (version < 32: width^2 pixels, resolution width - 1)
	uint32_t colorMapResolution = 0;
	std::vector<uint8_t> colorMap;
	std::vector<uint8_t> lightMap; // DDS, version 32+

	uint32_t textureMapResolution = 0;
	std::vector<uint8_t> textureMap; // textureMapResolution^2 RGBA pixels
	uint8_t textureSettings = 0; // version 32+
	std::vector<uint8_t> blendMap; // DDS, version 32+

	// Flair data
	std::vector<FlairAttributes> flairs;

	// The scene map is colorMapResolution * colorMapResolution scene IDs, indexed [x cell * resolution + z cell]
	std::vector<uint8_t> sceneMap;
	// Version 31 and older: the file's (colorMapResolution + 1)^2 scene map. In version 31 sceneMap is its first
	// colorMapResolution rows and columns; older versions don't use it and every cell is scene 0
	std::vector<uint8_t> sceneMapLegacy;

	// Mesh data (version 32+)
	uint32_t vertSize = 0;
	std::vector<uint16_t> meshVertUsage;
	std::vector<uint16_t> meshVertSize;
	std::vector<MeshTri> meshTri;

	// Scene map cells per world unit, and the size of one cell, along X and Z (set once the chunk is read)
	float sceneCellsPerUnitX = 0.0f;
	float sceneCellsPerUnitZ = 0.0f;
	float sceneCellSizeX = 0.0f;
	float sceneCellSizeZ = 0.0f;

	bool IsValidForSceneLookup() const;
	float GetMaxX() const { return offsetX + static_cast<float>(width - 1) * scaleFactor; }
	float GetMaxZ() const { return offsetZ + static_cast<float>(height - 1) * scaleFactor; }
	// The scene ID at a position relative to the chunk's offset, as the client looks it up
	uint8_t GetSceneIDAtLocal(float localX, float localZ) const;
	uint8_t GetSceneIDAtGrid(uint32_t i, uint32_t j) const;
	NiPoint3 GridToWorldPos(uint32_t i, uint32_t j) const;
};

/**
 * @brief RAW terrain file structure
 * Complete representation of a .raw terrain file
 */
struct Raw {
	uint16_t version = 0;
	uint8_t dev = 0;
	uint32_t numChunks = 0;
	uint32_t numChunksWidth = 0;
	uint32_t numChunksHeight = 0;
	std::vector<Chunk> chunks;
	
	// Calculated bounds of the entire terrain
	float minBoundsX = 0.0f;
	float minBoundsZ = 0.0f;
	float maxBoundsX = 0.0f;
	float maxBoundsZ = 0.0f;

	// Chunk lookup by position: the chunks of every client terrain form a regular grid, written X-major
	// ([x * chunks along Z + z]). chunkGrid holds the index into chunks per grid cell (-1 for none); it is empty when
	// the chunks don't form a grid, and lookups then check every chunk.
	std::vector<int32_t> chunkGrid;
	uint32_t chunkGridCountX = 0;
	uint32_t chunkGridCountZ = 0;
	float chunkSpanX = 0.0f;
	float chunkSpanZ = 0.0f;

	/**
	 * @brief The scene ID at a world position, looked up as the client's TerrainManager::GetSceneAtPos does
	 * (height is ignored). Constant time on a chunk grid.
	 *
	 * @return The scene ID, or nullopt when there is no terrain with a scene map there
	 */
	std::optional<uint8_t> GetSceneIDAt(float x, float z) const;
};

/**
 * @brief Read a RAW terrain file from an input stream
 * 
 * @param stream Input stream containing RAW file data
 * @param outRaw Output RAW file structure
 * @return true if successfully read, false otherwise
 */
bool ReadRaw(std::istream& stream, Raw& outRaw);

/**
 * @brief Generate a terrain mesh from raw chunks
 * Similar to dTerrain's GenerateFinalMeshFromChunks but creates a mesh with scene IDs
 * per vertex for fast scene lookups at arbitrary positions.
 * 
 * @param raw The RAW terrain data to generate mesh from
 * @param outMesh Output terrain mesh with vertices and scene IDs
 */
void GenerateTerrainMesh(const Raw& raw, TerrainMesh& outMesh);

/**
 * @brief Write terrain mesh to OBJ file for debugging/visualization
 * Merged from dTerrain's WriteFinalMeshToOBJ functionality
 * Vertices are colored based on their scene ID using a hash function
 * 
 * @param mesh The terrain mesh to export
 * @param path Output path for the OBJ file
 * @return true if successfully written, false otherwise
 */
bool WriteTerrainMeshToOBJ(const TerrainMesh& mesh, const std::string& path);

} // namespace Raw

#endif // __RAW_H__
