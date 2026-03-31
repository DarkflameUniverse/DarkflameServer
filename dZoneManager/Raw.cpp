#include "Raw.h"
#include "BinaryIO.h"
#include "Logger.h"
#include "SceneColor.h"
#include <fstream>
#include <algorithm>
#include <limits>

namespace {
constexpr uint32_t kMaxResolution = 4096;
constexpr size_t kMaxBlobBytes = 64ULL * 1024 * 1024; // 64 MiB
} // namespace

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
		BinaryIO::BinaryRead(stream, chunk.offsetX);
		BinaryIO::BinaryRead(stream, chunk.offsetZ);

		if (stream.fail()) {
			return false;
		}			// For version < 32, shader ID comes before texture IDs
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
			const size_t width = static_cast<size_t>(chunk.width);
			const size_t height = static_cast<size_t>(chunk.height);

			if (width == 0 || height == 0) {
				LOG("Chunk %u has invalid heightmap dimensions: width=%zu, height=%zu", chunk.id, width, height);
				return false;
			}

			if (width > kMaxResolution || height > kMaxResolution) {
				LOG("Chunk %u heightmap dimensions exceed maximum resolution %u: width=%zu, height=%zu", chunk.id, kMaxResolution, width, height);
				return false;
			}

			if (height != 0 && width > std::numeric_limits<size_t>::max() / height) {
				LOG("Chunk %u heightmap size multiplication overflows: width=%zu, height=%zu", chunk.id, width, height);
				return false;
			}

			const size_t heightMapSize = width * height;
			const size_t elementSize = sizeof(chunk.heightMap[0]);

			if (elementSize != 0 && heightMapSize > std::numeric_limits<size_t>::max() / elementSize) {
				LOG("Chunk %u heightmap byte size overflows: elements=%zu, elementSize=%zu", chunk.id, heightMapSize, elementSize);
				return false;
			}

			const size_t totalBytes = heightMapSize * elementSize;
			if (totalBytes == 0 || totalBytes > kMaxBlobBytes) {
				LOG("Chunk %u heightmap total size invalid: bytes=%zu (max %zu)", chunk.id, totalBytes, kMaxBlobBytes);
				return false;
			}

			chunk.heightMap.resize(heightMapSize);
			for (size_t i = 0; i < heightMapSize; ++i) {
				BinaryIO::BinaryRead(stream, chunk.heightMap[i]);
			}

			if (stream.fail()) {
				return false;
			}

			// ColorMap
			if (version >= 32) {
				BinaryIO::BinaryRead(stream, chunk.colorMapResolution);
			} else {
				chunk.colorMapResolution = chunk.width; // Default to chunk width for older versions
			}

			if (chunk.colorMapResolution > kMaxResolution) {
				LOG("Chunk colorMapResolution %u exceeds maximum %u", chunk.colorMapResolution, kMaxResolution);
				return false;
			}
			const size_t colorMapPixelCount = static_cast<size_t>(chunk.colorMapResolution) * chunk.colorMapResolution * 4; // RGBA
			if (colorMapPixelCount > kMaxBlobBytes) {
				LOG("Chunk colorMap size %zu exceeds maximum %zu bytes", colorMapPixelCount, kMaxBlobBytes);
				return false;
			}
			chunk.colorMap.resize(colorMapPixelCount);
			stream.read(reinterpret_cast<char*>(chunk.colorMap.data()), static_cast<std::streamsize>(colorMapPixelCount));

			if (stream.fail()) {
				return false;
			}
			// LightMap/diffusemap.dds
			uint32_t lightMapSize;
			BinaryIO::BinaryRead(stream, lightMapSize);

			if (lightMapSize > kMaxBlobBytes) {
				LOG("Chunk lightMap size %u exceeds maximum %zu bytes", lightMapSize, kMaxBlobBytes);
				return false;
			}
			chunk.lightMap.resize(lightMapSize);
			stream.read(reinterpret_cast<char*>(chunk.lightMap.data()), static_cast<std::streamsize>(lightMapSize));

			if (stream.fail()) {
				return false;
			}

			// TextureMap
			if (version >= 32) {
				BinaryIO::BinaryRead(stream, chunk.textureMapResolution);
			} else {
				chunk.textureMapResolution = chunk.width; // Default to chunk width for older versions
			}

			if (chunk.textureMapResolution > kMaxResolution) {
				LOG("Chunk textureMapResolution %u exceeds maximum %u", chunk.textureMapResolution, kMaxResolution);
				return false;
			}
			const size_t textureMapPixelCount = static_cast<size_t>(chunk.textureMapResolution) * chunk.textureMapResolution * 4;
			if (textureMapPixelCount > kMaxBlobBytes) {
				LOG("Chunk textureMap size %zu exceeds maximum %zu bytes", textureMapPixelCount, kMaxBlobBytes);
				return false;
			}
			chunk.textureMap.resize(textureMapPixelCount);
			stream.read(reinterpret_cast<char*>(chunk.textureMap.data()), static_cast<std::streamsize>(textureMapPixelCount));

			if (stream.fail()) {
				return false;
			}

			// Texture settings
			BinaryIO::BinaryRead(stream, chunk.textureSettings);

			// Blend map DDS
			uint32_t blendMapDDSSize;
			BinaryIO::BinaryRead(stream, blendMapDDSSize);

			if (blendMapDDSSize > kMaxBlobBytes) {
				LOG("Chunk blendMap size %u exceeds maximum %zu bytes", blendMapDDSSize, kMaxBlobBytes);
				return false;
			}
			chunk.blendMap.resize(blendMapDDSSize);
			stream.read(reinterpret_cast<char*>(chunk.blendMap.data()), static_cast<std::streamsize>(blendMapDDSSize));

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
				const size_t sceneMapSize = static_cast<size_t>(chunk.colorMapResolution) * chunk.colorMapResolution;

				if (sceneMapSize > kMaxBlobBytes) {
					LOG("Chunk sceneMap size %zu exceeds maximum %zu bytes", sceneMapSize, kMaxBlobBytes);
					return false;
				}
				chunk.sceneMap.resize(sceneMapSize);
				stream.read(reinterpret_cast<char*>(chunk.sceneMap.data()), static_cast<std::streamsize>(sceneMapSize));

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
				
				// Calculate terrain bounds from all chunks
			if (!outRaw.chunks.empty()) {
				outRaw.minBoundsX = std::numeric_limits<float>::max();
				outRaw.minBoundsZ = std::numeric_limits<float>::max();
				outRaw.maxBoundsX = std::numeric_limits<float>::lowest();
				outRaw.maxBoundsZ = std::numeric_limits<float>::lowest();
				
			for (const auto& chunk : outRaw.chunks) {
				// Calculate chunk bounds
				const float chunkMinX = chunk.offsetX;
				const float chunkMinZ = chunk.offsetZ;
				const float chunkMaxX = chunkMinX + (chunk.width * chunk.scaleFactor);
				const float chunkMaxZ = chunkMinZ + (chunk.height * chunk.scaleFactor);
				
				// Update overall bounds
				outRaw.minBoundsX = std::min(outRaw.minBoundsX, chunkMinX);
				outRaw.minBoundsZ = std::min(outRaw.minBoundsZ, chunkMinZ);
				outRaw.maxBoundsX = std::max(outRaw.maxBoundsX, chunkMaxX);
				outRaw.maxBoundsZ = std::max(outRaw.maxBoundsZ, chunkMaxZ);
			}
			LOG("Raw terrain bounds: X[%.2f, %.2f], Z[%.2f, %.2f]", 
				outRaw.minBoundsX, outRaw.maxBoundsX, outRaw.minBoundsZ, outRaw.maxBoundsZ);
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
					const float worldX = ((i) + (chunk.offsetX / chunk.scaleFactor)) * chunk.scaleFactor;
					const float worldY = (y / chunk.scaleFactor) * chunk.scaleFactor;
					const float worldZ = ((j) + (chunk.offsetZ / chunk.scaleFactor)) * chunk.scaleFactor;

					const NiPoint3 worldPos(worldX, worldY, worldZ);

					// Get scene ID at this position
					// Map heightmap position to scene map position
					// The scene map is colorMapResolution x colorMapResolution
					// We need to map from heightmap coordinates (i, j) to scene map coordinates
					const float sceneMapI = ((i) / (chunk.width - 1)) * (chunk.colorMapResolution - 1);
					const float sceneMapJ = ((j) / (chunk.height - 1)) * (chunk.colorMapResolution - 1);
					
					const uint32_t sceneI = std::min(static_cast<uint32_t>(sceneMapI), chunk.colorMapResolution - 1);
					const uint32_t sceneJ = std::min(static_cast<uint32_t>(sceneMapJ), chunk.colorMapResolution - 1);
					// Scene map uses the same indexing pattern as heightmap: row * width + col
					const uint32_t sceneIndex = sceneI * chunk.colorMapResolution + sceneJ;

					uint8_t sceneID = 0;
					if (sceneIndex < chunk.sceneMap.size()) {
						sceneID = chunk.sceneMap[sceneIndex];
					}					
					outMesh.vertices.emplace_back(worldPos, sceneID);
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
