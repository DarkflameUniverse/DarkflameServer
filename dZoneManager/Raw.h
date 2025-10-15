#pragma once

#ifndef __RAW_H__
#define __RAW_H__

#include <cstdint>
#include <vector>
#include <string>
#include <istream>
#include "NiPoint3.h"

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
	uint8_t colorA;
};

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
 * Represents a single chunk of terrain with heightmap, textures, and meshes
 */
struct Chunk {
	uint32_t id;
	uint32_t width;
	uint32_t height;
	float offsetWorldX;
	float offsetWorldZ;
	uint32_t shaderId;
	
	// Texture IDs (4 textures per chunk)
	std::vector<uint32_t> textureIds;
	
	// Terrain scale factor
	float scaleFactor;
	
	// Heightmap data (width * height floats)
	std::vector<float> heightMap;
	
	// Version 32+ fields
	uint32_t colorMapResolution = 0;
	std::vector<uint8_t> colorMap; // RGBA pixels (colorMap * colorMap * 4)
	std::vector<uint8_t> lightMap;
	
	uint32_t textureMapResolution = 0;
	std::vector<uint8_t> textureMap; // (textureMapResolution * textureMapResolution * 4)
	uint8_t textureSettings = 0;
	std::vector<uint8_t> blendMap;
	
	// Flair data
	std::vector<FlairAttributes> flairs;
	
	// Scene map (version 32+)
	std::vector<uint8_t> sceneMap;
	
	// Mesh data
	uint32_t vertSize = 0;
	std::vector<uint16_t> meshVertUsage;
	std::vector<uint16_t> meshVertSize;
	std::vector<MeshTri> meshTri;
	
	// Unknown data for version < 32
	std::vector<uint8_t> unknown1;
	std::vector<uint8_t> unknown2;
};

/**
 * @brief RAW terrain file structure
 * Complete representation of a .raw terrain file
 */
struct Raw {
	uint16_t version;
	uint8_t dev;
	uint32_t numChunks = 0;
	uint32_t numChunksWidth = 0;
	uint32_t numChunksHeight = 0;
	std::vector<Chunk> chunks;
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

} // namespace Raw

#endif // __RAW_H__
