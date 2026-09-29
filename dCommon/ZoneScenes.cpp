#include "ZoneScenes.h"

#include <algorithm>
#include <cmath>

namespace ZoneScenes {
	namespace {
		// The client's margin on chunk edges (FLOAT_01479f24)
		constexpr float EDGE = 0.001f;
	}

	SceneMap::SceneMap(const Raw::Raw& raw) {
		bool first = true;
		for (const auto& chunk : raw.chunks) {
			if (!chunk.IsValidForSceneLookup() || chunk.sceneMap.size() < static_cast<size_t>(chunk.colorMapResolution) * chunk.colorMapResolution) continue;
			Chunk out;
			out.minX = chunk.offsetX;
			out.minZ = chunk.offsetZ;
			out.maxX = chunk.offsetX + static_cast<float>(chunk.width - 1) * chunk.scaleFactor;
			out.maxZ = chunk.offsetZ + static_cast<float>(chunk.height - 1) * chunk.scaleFactor;
			out.resolution = chunk.colorMapResolution;
			out.cellsPerUnitX = static_cast<float>(chunk.colorMapResolution) / (static_cast<float>(chunk.width - 1) * chunk.scaleFactor);
			out.cellsPerUnitZ = static_cast<float>(chunk.colorMapResolution) / (static_cast<float>(chunk.height - 1) * chunk.scaleFactor);
			out.scenes = chunk.sceneMap;
			m_MinX = first ? out.minX : std::min(m_MinX, out.minX);
			m_MinZ = first ? out.minZ : std::min(m_MinZ, out.minZ);
			m_MaxX = first ? out.maxX : std::max(m_MaxX, out.maxX);
			m_MaxZ = first ? out.maxZ : std::max(m_MaxZ, out.maxZ);
			first = false;
			m_Chunks.push_back(std::move(out));
		}
	}

	uint32_t SceneMap::SceneAt(float x, float z) const {
		if (m_Chunks.empty() || !std::isfinite(x) || !std::isfinite(z)) return GLOBAL_SCENE;
		// As TerrainManager's chunk lookup (0x01065c00): clamped to the terrain, then to one cell inside its far edge
		x = std::clamp(x, m_MinX, m_MaxX);
		z = std::clamp(z, m_MinZ, m_MaxZ);
		for (const auto& chunk : m_Chunks) {
			const auto px = std::min(x, m_MaxX - 1.0f / chunk.cellsPerUnitX);
			const auto pz = std::min(z, m_MaxZ - 1.0f / chunk.cellsPerUnitZ);
			if (px < chunk.minX - EDGE || px >= chunk.maxX - EDGE || pz < chunk.minZ - EDGE || pz >= chunk.maxZ - EDGE) continue;
			const auto last = static_cast<int64_t>(chunk.resolution) - 1;
			const auto cellX = std::clamp(static_cast<int64_t>(std::floor(chunk.cellsPerUnitX * (px - chunk.minX) + 0.5f)), int64_t{ 0 }, last);
			const auto cellZ = std::clamp(static_cast<int64_t>(std::floor(chunk.cellsPerUnitZ * (pz - chunk.minZ) + 0.5f)), int64_t{ 0 }, last);
			const auto scene = chunk.scenes[static_cast<size_t>(cellX * chunk.resolution + cellZ)];
			return scene == NO_SCENE ? GLOBAL_SCENE : scene;
		}
		return GLOBAL_SCENE;
	}

	SceneGraph::SceneGraph(const std::vector<ZoneScene>& scenes, const std::vector<SceneTransition>& transitions) {
		for (const auto& scene : scenes) m_Scenes.insert(scene.id);
		const auto link = [this](uint32_t from, uint32_t to) {
			auto it = std::find_if(m_Neighbours.begin(), m_Neighbours.end(), [from](const auto& entry) { return entry.first == from; });
			if (it == m_Neighbours.end()) it = m_Neighbours.insert(m_Neighbours.end(), { from, {} });
			it->second.insert(to);
		};
		for (const auto& transition : transitions) {
			if (transition.points.size() < 2) continue;
			// The low half of a point's LWOSCENEID is the scene, the high half its layer
			const auto a = static_cast<uint32_t>(transition.points[0].sceneID & 0xFFFFFFFF);
			const auto b = static_cast<uint32_t>(transition.points[1].sceneID & 0xFFFFFFFF);
			if (a == b || !m_Scenes.contains(a) || !m_Scenes.contains(b)) continue;
			link(a, b);
			link(b, a);
		}
	}

	const std::set<uint32_t>& SceneGraph::Neighbours(uint32_t scene) const {
		static const std::set<uint32_t> NONE;
		if (scene == GLOBAL_SCENE) return NONE;
		const auto it = std::find_if(m_Neighbours.begin(), m_Neighbours.end(), [scene](const auto& entry) { return entry.first == scene; });
		return it == m_Neighbours.end() ? NONE : it->second;
	}

	std::set<uint32_t> SceneGraph::Loaded(uint32_t scene) const {
		std::set<uint32_t> loaded{ GLOBAL_SCENE };
		if (scene == GLOBAL_SCENE) return loaded;
		loaded.insert(scene);
		const auto& neighbours = Neighbours(scene);
		loaded.insert(neighbours.begin(), neighbours.end());
		return loaded;
	}

	std::set<uint32_t> SceneGraph::Loaded(uint32_t referenceScene, uint32_t positionScene, bool referenceOverridden) const {
		auto loaded = Loaded(referenceScene);
		if (referenceOverridden) loaded.merge(Loaded(positionScene));
		return loaded;
	}
}

namespace ZoneScenes {
	std::string RunLengths(const std::vector<uint8_t>& cells, size_t count) {
		count = std::min(count, cells.size());
		std::string runs;
		for (size_t i = 0; i < count;) {
			size_t length = 1;
			while (i + length < count && length < 255 && cells[i + length] == cells[i]) length++;
			runs.push_back(static_cast<char>(length));
			runs.push_back(static_cast<char>(cells[i]));
			i += length;
		}
		return runs;
	}
}
