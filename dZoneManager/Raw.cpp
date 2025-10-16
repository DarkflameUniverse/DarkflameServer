#include "Raw.h"
#include "BinaryIO.h"
#include "Logger.h"
#include "SceneColor.h"
#include <fstream>
#include <algorithm>

namespace Raw {

	/**
	 * @brief Read flair attributes from stream
	 */
	static bool ReadFlairAttributes(std::istream& stream, FlairAttributes& flair) {
		try {
			BinaryIO::BinaryRead(stream, flair.id);
			BinaryIO::BinaryRead(stream, flair.scaleFactor);
			BinaryIO::BinaryRead(stream, flair.position.x);
			BinaryIO::BinaryRead(stream, flair.position.y);
			BinaryIO::BinaryRead(stream, flair.position.z);
			BinaryIO::BinaryRead(stream, flair.rotation.x);
			BinaryIO::BinaryRead(stream, flair.rotation.y);
			BinaryIO::BinaryRead(stream, flair.rotation.z);
			BinaryIO::BinaryRead(stream, flair.colorR);
			BinaryIO::BinaryRead(stream, flair.colorG);
			BinaryIO::BinaryRead(stream, flair.colorB);
			BinaryIO::BinaryRead(stream, flair.colorA);
			return true;
		} catch (const std::exception&) {
			return false;
		}
	}

	/**
	 * @brief Read mesh triangle data from stream
	 */
	static bool ReadMeshTri(std::istream& stream, MeshTri& meshTri) {
		try {
			BinaryIO::BinaryRead(stream, meshTri.meshTriListSize);
			meshTri.meshTriList.resize(meshTri.meshTriListSize);
			for (uint16_t i = 0; i < meshTri.meshTriListSize; ++i) {
				BinaryIO::BinaryRead(stream, meshTri.meshTriList[i]);
			}
			return true;
		} catch (const std::exception&) {
			return false;
		}
	}

	/**
	 * @brief Read a chunk from stream
	 */
	static bool ReadChunk(std::istream& stream, Chunk& chunk, uint16_t version) {
		try {
			// Read basic chunk info
			BinaryIO::BinaryRead(stream, chunk.id);
			if (stream.fail()) {
				return false;
			}

			BinaryIO::BinaryRead(stream, chunk.width);
			BinaryIO::BinaryRead(stream, chunk.height);
			BinaryIO::BinaryRead(stream, chunk.offsetWorldX);
			BinaryIO::BinaryRead(stream, chunk.offsetWorldZ);

			if (stream.fail()) {
				return false;
			}

			// For version < 32, shader ID comes before texture IDs
			if (version < 32) {
				BinaryIO::BinaryRead(stream, chunk.shaderId);
			}

			// Read texture IDs (4 textures)
			chunk.textureIds.resize(4);
			for (int i = 0; i < 4; ++i) {
				BinaryIO::BinaryRead(stream, chunk.textureIds[i]);
			}

			if (stream.fail()) {
				return false;
			}

			// Read scale factor
			BinaryIO::BinaryRead(stream, chunk.scaleFactor);

			if (stream.fail()) {
				return false;
			}

			// Read heightmap
			uint32_t heightMapSize = chunk.width * chunk.height;

			chunk.heightMap.resize(heightMapSize);
			for (uint32_t i = 0; i < heightMapSize; ++i) {
				BinaryIO::BinaryRead(stream, chunk.heightMap[i]);
			}

			if (stream.fail()) {
				return false;
			}

			// ColorMap (size varies by version)
			if (version >= 32) {
				BinaryIO::BinaryRead(stream, chunk.colorMapResolution);
			} else {
				chunk.colorMapResolution = chunk.width; // Default to chunk width for older versions
			}

			uint32_t colorMapPixelCount = chunk.colorMapResolution * chunk.colorMapResolution * 4; // RGBA
			chunk.colorMap.resize(colorMapPixelCount);
			stream.read(reinterpret_cast<char*>(chunk.colorMap.data()), colorMapPixelCount);

			if (stream.fail()) {
				return false;
			}

			// LightMap DDS
			uint32_t lightMapSize;
			BinaryIO::BinaryRead(stream, lightMapSize);

			chunk.lightMap.resize(lightMapSize);
			stream.read(reinterpret_cast<char*>(chunk.lightMap.data()), lightMapSize);

			if (stream.fail()) {
				return false;
			}

			// TextureMap (size varies by version)
			if (version >= 32) {
				BinaryIO::BinaryRead(stream, chunk.textureMapResolution);
			} else {
				chunk.textureMapResolution = chunk.width; // Default to chunk width for older versions
			}

			uint32_t textureMapPixelCount = chunk.textureMapResolution * chunk.textureMapResolution * 4;
			chunk.textureMap.resize(textureMapPixelCount);
			stream.read(reinterpret_cast<char*>(chunk.textureMap.data()), textureMapPixelCount);

			if (stream.fail()) {
				return false;
			}

			// Texture settings
			BinaryIO::BinaryRead(stream, chunk.textureSettings);

			// Blend map DDS
			uint32_t blendMapDDSSize;
			BinaryIO::BinaryRead(stream, blendMapDDSSize);

			chunk.blendMap.resize(blendMapDDSSize);
			stream.read(reinterpret_cast<char*>(chunk.blendMap.data()), blendMapDDSSize);

			if (stream.fail()) {
				return false;
			}

			// Read flairs
			uint32_t numFlairs;
			BinaryIO::BinaryRead(stream, numFlairs);

			if (stream.fail()) {
				return false;
			}

			chunk.flairs.resize(numFlairs);
			for (uint32_t i = 0; i < numFlairs; ++i) {
				if (!ReadFlairAttributes(stream, chunk.flairs[i])) {
					return false;
				}
			}

			// Scene map (version 32+ only)
			if (version >= 32) {
				uint32_t sceneMapSize = chunk.colorMapResolution * chunk.colorMapResolution;

				chunk.sceneMap.resize(sceneMapSize);
				stream.read(reinterpret_cast<char*>(chunk.sceneMap.data()), sceneMapSize);

				if (stream.fail()) {
					return false;
				}
			}

			// Mesh vertex usage (read size first, then check if empty)
			BinaryIO::BinaryRead(stream, chunk.vertSize);

			if (stream.fail()) {
				return false;
			}

			// Mesh vert usage
			chunk.meshVertUsage.resize(chunk.vertSize);
			for (uint32_t i = 0; i < chunk.vertSize; ++i) {
				BinaryIO::BinaryRead(stream, chunk.meshVertUsage[i]);
			}

			if (stream.fail()) {
				return false;
			}

			// Only continue with mesh data if we have vertex usage data
			if (chunk.vertSize == 0) {
				return true;
			}

			// Mesh vert size (16 elements)
			chunk.meshVertSize.resize(16);
			for (int i = 0; i < 16; ++i) {
				BinaryIO::BinaryRead(stream, chunk.meshVertSize[i]);
			}

			if (stream.fail()) {
				return false;
			}

			// Mesh triangles (16 elements)
			chunk.meshTri.resize(16);
			for (int i = 0; i < 16; ++i) {
				if (!ReadMeshTri(stream, chunk.meshTri[i])) {
					return false;
				}
			}

			return true;
		} catch (const std::exception&) {
			return false;
		}
	}

	bool ReadRaw(std::istream& stream, Raw& outRaw) {
		// Get stream size
		stream.seekg(0, std::ios::end);
		auto streamSize = stream.tellg();
		stream.seekg(0, std::ios::beg);

		if (streamSize <= 0) {
			return false;
		}

		try {
			// Read header
			BinaryIO::BinaryRead(stream, outRaw.version);

			if (stream.fail()) {
				return false;
			}

			BinaryIO::BinaryRead(stream, outRaw.dev);

			if (stream.fail()) {
				return false;
			}

			// Only read chunks if dev == 0
			if (outRaw.dev == 0) {
				BinaryIO::BinaryRead(stream, outRaw.numChunks);
				BinaryIO::BinaryRead(stream, outRaw.numChunksWidth);
				BinaryIO::BinaryRead(stream, outRaw.numChunksHeight);

				// Read all chunks
				outRaw.chunks.resize(outRaw.numChunks);
				for (uint32_t i = 0; i < outRaw.numChunks; ++i) {
					if (!ReadChunk(stream, outRaw.chunks[i], outRaw.version)) {
						return false;
					}
				}
			}

			return true;
		} catch (const std::exception&) {
			return false;
		}
	}

	void GenerateTerrainMesh(const Raw& raw, TerrainMesh& outMesh) {
		outMesh.vertices.clear();
		outMesh.triangles.clear();

		if (raw.chunks.empty() || raw.version < 32) {
			return; // No scene data available
		}

		LOG("GenerateTerrainMesh: Processing %u chunks", raw.chunks.size());

		uint32_t vertexOffset = 0;

		for (const auto& chunk : raw.chunks) {
			// Skip chunks without scene maps
			if (chunk.sceneMap.empty() || chunk.colorMapResolution == 0 || chunk.heightMap.empty()) {
				LOG("Skipping chunk %u (sceneMap: %zu, colorMapRes: %u, heightMap: %zu)", 
					chunk.id, chunk.sceneMap.size(), chunk.colorMapResolution, chunk.heightMap.size());
				continue;
			}

			LOG("Processing chunk %u: width=%u, height=%u, colorMapRes=%u, sceneMapSize=%zu", 
				chunk.id, chunk.width, chunk.height, chunk.colorMapResolution, chunk.sceneMap.size());

			// Generate vertices for this chunk
			for (uint32_t i = 0; i < chunk.width; ++i) {
				for (uint32_t j = 0; j < chunk.height; ++j) {
					// Get height at this position
					const uint32_t heightIndex = chunk.width * i + j;
					if (heightIndex >= chunk.heightMap.size()) continue;
					
					const float y = chunk.heightMap[heightIndex];

					// Calculate world position
					// Based on RawFile::GenerateFinalMeshFromChunks in dTerrain:
					// tempVert.SetX(tempVert.GetX() + (chunk->m_X / chunk->m_HeightMap->m_ScaleFactor));
					// tempVert.SetY(tempVert.GetY() / chunk->m_HeightMap->m_ScaleFactor);
					// tempVert.SetZ(tempVert.GetZ() + (chunk->m_Z / chunk->m_HeightMap->m_ScaleFactor));
					// tempVert *= chunk->m_HeightMap->m_ScaleFactor;
					
					float worldX = (static_cast<float>(i) + (chunk.offsetWorldX / chunk.scaleFactor)) * chunk.scaleFactor;
					float worldY = (y / chunk.scaleFactor) * chunk.scaleFactor;
					float worldZ = (static_cast<float>(j) + (chunk.offsetWorldZ / chunk.scaleFactor)) * chunk.scaleFactor;

					NiPoint3 worldPos(worldX, worldY, worldZ);

				// Get scene ID at this position
				// Map heightmap position to scene map position
				// The scene map is colorMapResolution x colorMapResolution
				// We need to map from heightmap coordinates (i, j) to scene map coordinates
				const float sceneMapI = (static_cast<float>(i) / static_cast<float>(chunk.width - 1)) * static_cast<float>(chunk.colorMapResolution - 1);
				const float sceneMapJ = (static_cast<float>(j) / static_cast<float>(chunk.height - 1)) * static_cast<float>(chunk.colorMapResolution - 1);
				
				const uint32_t sceneI = std::min(static_cast<uint32_t>(sceneMapI), chunk.colorMapResolution - 1);
				const uint32_t sceneJ = std::min(static_cast<uint32_t>(sceneMapJ), chunk.colorMapResolution - 1);
				// Scene map uses the same indexing pattern as heightmap: row * width + col
				const uint32_t sceneIndex = sceneI * chunk.colorMapResolution + sceneJ;

				uint8_t sceneID = 0;
				if (sceneIndex < chunk.sceneMap.size()) {
					sceneID = chunk.sceneMap[sceneIndex];
				}					
				outMesh.vertices.emplace_back(worldPos, sceneID);

					// Generate triangles (same pattern as dTerrain)
					if (i > 0 && j > 0) {
						const uint32_t currentVert = vertexOffset + chunk.width * i + j;
						const uint32_t leftVert = currentVert - 1;
						const uint32_t bottomLeftVert = vertexOffset + chunk.width * (i - 1) + j - 1;
						const uint32_t bottomVert = vertexOffset + chunk.width * (i - 1) + j;

						// First triangle
						outMesh.triangles.push_back(currentVert);
						outMesh.triangles.push_back(leftVert);
						outMesh.triangles.push_back(bottomLeftVert);

						// Second triangle
						outMesh.triangles.push_back(bottomLeftVert);
						outMesh.triangles.push_back(bottomVert);
						outMesh.triangles.push_back(currentVert);
					}
				}
			}

		vertexOffset += chunk.width * chunk.height;
	}
}

bool WriteTerrainMeshToOBJ(const TerrainMesh& mesh, const std::string& path) {
	try {
		std::ofstream file(path);
		if (!file.is_open()) {
			LOG("Failed to open OBJ file for writing: %s", path.c_str());
			return false;
		}

		// Create instance of SceneColor for color lookup
		SceneColor sceneColor;

		// Write vertices with colors
		// OBJ format supports vertex colors as: v x y z r g b
		for (const auto& v : mesh.vertices) {
			file << "v " << v.position.x << ' ' << v.position.y << ' ' << v.position.z;
			
			uint8_t sceneID = v.sceneID;

			const NiColor& color = sceneColor.Get(sceneID);
			file << ' ' << color.m_Red << ' ' << color.m_Green << ' ' << color.m_Blue;
			file << '\n';
		}

		// Write faces (triangles)
		for (size_t i = 0; i < mesh.triangles.size(); i += 3) {
			// OBJ indices are 1-based
			file << "f " << (mesh.triangles[i] + 1) << ' ' 
			     << (mesh.triangles[i + 1] + 1) << ' ' 
			     << (mesh.triangles[i + 2] + 1) << '\n';
		}

		file.close();
		LOG("Successfully wrote terrain mesh to OBJ: %s (%zu vertices, %zu triangles)", 
		    path.c_str(), mesh.vertices.size(), mesh.triangles.size() / 3);
		return true;
	} catch (const std::exception& e) {
		LOG("Exception while writing OBJ file: %s", e.what());
		return false;
	}
}

} // namespace Raw
