#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "Raw.h"

/**
 * A zone's terrain (.raw) as a single top-down height grid for the dashboard's map reports. The file is read by Raw
 * (dCommon), which the 3D views use too; the rest is pure (no files or database) so it can be unit tested.
 *
 * Each chunk has a world offset (x, z), a vertex grid (width x height), a scale (world units between vertices) and
 * heights where heightMap[width * i + j] is the vertex at x = offsetX + i * scale, z = offsetZ + j * scale. Checked
 * against object positions in Avant Gardens.
 */
namespace TerrainMap {
	struct Grid {
		float minX{};
		float minZ{};
		float step{};       // world units between grid samples
		uint32_t width{};   // samples along x
		uint32_t height{};  // samples along z
		float minY{};
		float maxY{};
		std::vector<float> heights; // row-major by z: heights[z * width + x]; NaN where there is no terrain
	};

	// A .raw file read whole (Raw::ReadRaw); nullopt when it's damaged or has no chunks. Takes the bytes to avoid a copy.
	inline std::optional<Raw::Raw> Read(std::string data) {
		std::istringstream stream(std::move(data));
		Raw::Raw raw;
		if (!Raw::ReadRaw(stream, raw) || raw.chunks.empty()) return std::nullopt;
		for (const auto& chunk : raw.chunks) {
			// Every use divides by the scale and walks the heights
			if (!(chunk.scaleFactor > 0.0f) || chunk.width == 0 || chunk.height == 0 || chunk.heightMap.size() != static_cast<size_t>(chunk.width) * chunk.height) return std::nullopt;
		}
		return raw;
	}

	/**
	 * The terrain as a grid, sampling every `stride` vertices so large zones stay small.
	 * @param maxSamples the grid is thinned until neither side exceeds this
	 */
	inline std::optional<Grid> Parse(const Raw::Raw& raw, uint32_t maxSamples = 512) {
		const auto& chunks = raw.chunks;
		if (chunks.empty()) return std::nullopt;
		const float scale = chunks.front().scaleFactor;
		Grid grid;
		grid.minX = chunks.front().offsetX;
		grid.minZ = chunks.front().offsetZ;
		float maxX = grid.minX, maxZ = grid.minZ;
		for (const auto& chunk : chunks) {
			grid.minX = std::min(grid.minX, chunk.offsetX);
			grid.minZ = std::min(grid.minZ, chunk.offsetZ);
			maxX = std::max(maxX, chunk.offsetX + (chunk.width - 1) * chunk.scaleFactor);
			maxZ = std::max(maxZ, chunk.offsetZ + (chunk.height - 1) * chunk.scaleFactor);
		}

		const auto verticesX = static_cast<uint32_t>(std::lround((maxX - grid.minX) / scale)) + 1;
		const auto verticesZ = static_cast<uint32_t>(std::lround((maxZ - grid.minZ) / scale)) + 1;
		uint32_t stride = 1;
		while ((verticesX + stride - 1) / stride > maxSamples || (verticesZ + stride - 1) / stride > maxSamples) stride++;

		grid.step = scale * stride;
		grid.width = (verticesX + stride - 1) / stride;
		grid.height = (verticesZ + stride - 1) / stride;
		grid.heights.assign(static_cast<size_t>(grid.width) * grid.height, std::nanf(""));
		grid.minY = INFINITY;
		grid.maxY = -INFINITY;

		for (const auto& chunk : chunks) {
			const auto offsetX = static_cast<int64_t>(std::lround((chunk.offsetX - grid.minX) / scale));
			const auto offsetZ = static_cast<int64_t>(std::lround((chunk.offsetZ - grid.minZ) / scale));
			for (uint32_t i = 0; i < chunk.width; i++) {
				const auto vx = offsetX + static_cast<int64_t>(std::lround(i * chunk.scaleFactor / scale));
				if (vx < 0 || vx % stride != 0) continue;
				for (uint32_t j = 0; j < chunk.height; j++) {
					const auto vz = offsetZ + static_cast<int64_t>(std::lround(j * chunk.scaleFactor / scale));
					if (vz < 0 || vz % stride != 0) continue;
					const auto gx = static_cast<uint32_t>(vx / stride), gz = static_cast<uint32_t>(vz / stride);
					if (gx >= grid.width || gz >= grid.height) continue;
					const float y = chunk.heightMap[static_cast<size_t>(chunk.width) * i + j];
					if (!std::isfinite(y)) continue;
					grid.heights[static_cast<size_t>(gz) * grid.width + gx] = y;
					grid.minY = std::min(grid.minY, y);
					grid.maxY = std::max(grid.maxY, y);
				}
			}
		}
		if (!std::isfinite(grid.minY)) return std::nullopt;
		return grid;
	}

	// Parse a .raw file's bytes into a grid
	inline std::optional<Grid> Parse(std::string_view data, uint32_t maxSamples = 512) {
		const auto raw = Read(std::string(data));
		return raw ? Parse(*raw, maxSamples) : std::nullopt;
	}

	/**
	 * Heights quantized to 16 bits (0 = minY, 65534 = maxY, 65535 = no terrain), little-endian, for sending to the
	 * browser, which shades them itself.
	 */
	inline std::string Quantize(const Grid& grid) {
		std::string out;
		out.resize(grid.heights.size() * 2);
		const float range = grid.maxY - grid.minY;
		for (size_t i = 0; i < grid.heights.size(); i++) {
			const float y = grid.heights[i];
			uint16_t value = 65535;
			if (std::isfinite(y)) value = static_cast<uint16_t>(range > 0 ? std::lround((y - grid.minY) / range * 65534.0f) : 0);
			out[i * 2] = static_cast<char>(value & 0xFF);
			out[i * 2 + 1] = static_cast<char>(value >> 8);
		}
		return out;
	}
}
