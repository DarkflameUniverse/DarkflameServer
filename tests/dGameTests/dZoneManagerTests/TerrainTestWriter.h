#pragma once

#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

#include "NiPoint3.h"

// Writes .raw terrain and .luz zone files in the client's layout, for the zone manager tests
namespace TerrainTestWriter {
	template<typename T>
	void Write(std::ostream& stream, const T& value) {
		stream.write(reinterpret_cast<const char*>(&value), sizeof(T));
	}

	inline void WriteBytes(std::ostream& stream, size_t count, uint8_t value = 0xAB) {
		const std::string bytes(count, static_cast<char>(value));
		stream.write(bytes.data(), bytes.size());
	}

	inline void WriteString8(std::ostream& stream, const std::string& value) {
		Write<uint8_t>(stream, static_cast<uint8_t>(value.size()));
		stream.write(value.data(), value.size());
	}

	struct TestChunk {
		uint32_t id = 0;
		uint32_t width = 3;
		uint32_t height = 3;
		float offsetX = 0.0f;
		float offsetZ = 0.0f;
		float scale = 1.0f;
		// Version 32+: the color map resolution; older versions use width - 1
		uint32_t colorMapResolution = 2;
		// colorMapResolution^2 scene IDs ((resolution + 1)^2 for version 31 and older); empty = all 0
		std::vector<uint8_t> sceneMap;
		// width * height heights; empty = the point index
		std::vector<float> heights;
		uint32_t lightMapSize = 5;
		uint32_t textureMapResolution = 2;
		uint32_t blendMapSize = 7;
		uint32_t flairCount = 2;
		uint32_t vertSize = 4;
		std::vector<uint16_t> triangleListSizes = { 3, 0, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3 };
	};

	inline void WriteChunk(std::ostream& stream, const TestChunk& chunk, uint16_t version) {
		Write(stream, chunk.id);
		Write(stream, chunk.width);
		Write(stream, chunk.height);
		Write(stream, chunk.offsetX);
		Write(stream, chunk.offsetZ);
		if (version < 32) Write<uint32_t>(stream, 77); // shader ID
		for (uint32_t i = 0; i < 4; ++i) Write<uint32_t>(stream, 100 + i); // texture IDs
		Write(stream, chunk.scale);
		for (uint32_t i = 0; i < chunk.width * chunk.height; ++i) {
			Write<float>(stream, chunk.heights.empty() ? static_cast<float>(i) : chunk.heights[i]);
		}

		const uint32_t resolution = version >= 32 ? chunk.colorMapResolution : chunk.width - 1;
		if (version >= 32) {
			Write(stream, resolution);
			WriteBytes(stream, static_cast<size_t>(resolution) * resolution * 4);
			Write(stream, chunk.lightMapSize);
			WriteBytes(stream, chunk.lightMapSize);
		} else {
			WriteBytes(stream, static_cast<size_t>(chunk.width) * chunk.width * 4);
		}

		Write(stream, chunk.textureMapResolution);
		WriteBytes(stream, static_cast<size_t>(chunk.textureMapResolution) * chunk.textureMapResolution * 4);
		if (version >= 32) {
			Write<uint8_t>(stream, 1); // texture settings
			Write(stream, chunk.blendMapSize);
			WriteBytes(stream, chunk.blendMapSize);
		}

		// Flair: id, scale, position, rotation, RGB, padding byte (36 bytes)
		Write(stream, chunk.flairCount);
		for (uint32_t i = 0; i < chunk.flairCount; ++i) {
			Write<uint32_t>(stream, 500 + i);
			Write<float>(stream, 1.5f);
			Write(stream, NiPoint3(1.0f + i, 2.0f, 3.0f));
			Write(stream, NiPoint3(0.0f, 90.0f, 0.0f));
			for (const uint8_t channel : { 10, 20, 30, 40 }) Write(stream, channel);
		}

		// version 31 and older: one more row and column
		const size_t cells = version >= 32 ? static_cast<size_t>(resolution) * resolution : static_cast<size_t>(resolution + 1) * (resolution + 1);
		for (size_t i = 0; i < cells; ++i) Write<uint8_t>(stream, chunk.sceneMap.empty() ? 0 : chunk.sceneMap[i]);

		if (version < 32) return;
		Write(stream, chunk.vertSize);
		if (chunk.vertSize == 0) return;
		WriteBytes(stream, static_cast<size_t>(chunk.vertSize) * sizeof(uint16_t));
		WriteBytes(stream, 16 * sizeof(uint16_t));
		for (uint32_t i = 0; i < 16; ++i) {
			const uint16_t size = i < chunk.triangleListSizes.size() ? chunk.triangleListSizes[i] : 0;
			Write(stream, size);
			WriteBytes(stream, static_cast<size_t>(size) * sizeof(uint16_t));
		}
	}

	inline std::string WriteRaw(const std::vector<TestChunk>& chunks, uint16_t version = 32, uint32_t chunksX = 1, uint32_t chunksZ = 1, uint8_t dev = 0) {
		std::ostringstream stream;
		Write(stream, version);
		Write(stream, dev);
		if (dev == 0) {
			Write<uint32_t>(stream, static_cast<uint32_t>(chunks.size()));
			Write(stream, chunksZ); // the header's "width" is the chunk count along Z
			Write(stream, chunksX);
			for (const auto& chunk : chunks) WriteChunk(stream, chunk, version);
		}
		return stream.str();
	}

	struct TestScene {
		std::string filename;
		uint32_t id = 0;
		uint32_t layer = 0;
		std::string name;
		uint8_t r = 0, g = 0, b = 0;
	};

	struct TestTransitionPoint {
		uint32_t sceneID = 0;
		uint32_t layerID = 0;
		NiPoint3 position;
	};

	// A .luz (version 36 to 41) with no paths
	inline std::string WriteLuz(uint32_t worldID, const std::vector<TestScene>& scenes, uint8_t boundaryCount, const std::string& rawName,
		const std::vector<std::pair<TestTransitionPoint, TestTransitionPoint>>& transitions, uint32_t version = 41) {
		std::ostringstream stream;
		Write(stream, version);
		Write<uint32_t>(stream, 5); // revision
		Write(stream, worldID);
		if (version >= 38) {
			Write(stream, NiPoint3(1.0f, 2.0f, 3.0f)); // spawn position
			for (const float value : { 1.0f, 0.0f, 0.0f, 0.0f }) Write(stream, value); // spawn rotation
		}
		// the scene count is a byte before version 37
		if (version >= 37) Write<uint32_t>(stream, static_cast<uint32_t>(scenes.size()));
		else Write<uint8_t>(stream, static_cast<uint8_t>(scenes.size()));
		for (const auto& scene : scenes) {
			WriteString8(stream, scene.filename);
			Write(stream, scene.id);
			Write(stream, scene.layer);
			WriteString8(stream, scene.name);
			Write(stream, scene.r);
			Write(stream, scene.b);
			Write(stream, scene.g);
		}

		// Zone boundary lines: normal, point, destination zone, destination scene, spawn location (44 bytes)
		Write(stream, boundaryCount);
		for (uint8_t i = 0; i < boundaryCount; ++i) {
			Write(stream, NiPoint3(0.0f, 0.0f, 1.0f));
			Write(stream, NiPoint3(10.0f, 0.0f, static_cast<float>(i)));
			Write<uint16_t>(stream, 1100); // map
			Write<uint16_t>(stream, i + 1); // instance
			Write<uint32_t>(stream, 2);
			Write(stream, NiPoint3(5.0f, 6.0f, 7.0f));
		}

		WriteString8(stream, rawName);
		WriteString8(stream, "Test Zone");
		WriteString8(stream, "A zone for tests");

		Write<uint32_t>(stream, static_cast<uint32_t>(transitions.size()));
		for (const auto& [from, to] : transitions) {
			// before version 40 a transition has a name and a width
			if (version < 40) {
				WriteString8(stream, "transition");
				Write(stream, 2.5f);
			}
			// five points from version 34 to 38 (the last three repeat the second here), two otherwise
			const size_t pointCount = version >= 34 && version < 39 ? 5 : 2;
			for (size_t i = 0; i < pointCount; ++i) {
				const auto& point = i == 0 ? from : to;
				Write(stream, point.sceneID);
				Write(stream, point.layerID);
				Write(stream, point.position);
			}
		}

		Write<uint32_t>(stream, 8); // path data length
		Write<uint32_t>(stream, 1); // path chunk version
		Write<uint32_t>(stream, 0); // path count
		return stream.str();
	}
}
