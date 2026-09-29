#ifndef LWOSCENEID_H
#define LWOSCENEID_H

#include <cstdint>

#include "eSceneType.h"

// A scene of a zone: its scene ID and layer
struct LWOSCENEID {
public:
	constexpr LWOSCENEID() noexcept { m_sceneID = -1; m_layerID = eSceneType::General; }
	constexpr LWOSCENEID(int32_t sceneID) noexcept { m_sceneID = sceneID; m_layerID = eSceneType::General; }
	constexpr LWOSCENEID(int32_t sceneID, eSceneType layerID) noexcept { m_sceneID = sceneID; m_layerID = layerID; }

	constexpr LWOSCENEID& operator=(const LWOSCENEID& rhs) noexcept { m_sceneID = rhs.m_sceneID; m_layerID = rhs.m_layerID; return *this; }
	constexpr LWOSCENEID& operator=(const int32_t rhs) noexcept { m_sceneID = rhs; m_layerID = eSceneType::General; return *this; }

	constexpr bool operator<(const LWOSCENEID& rhs) const noexcept { return (m_sceneID < rhs.m_sceneID || (m_sceneID == rhs.m_sceneID && m_layerID < rhs.m_layerID)); }
	constexpr bool operator<(const int32_t rhs) const noexcept { return m_sceneID < rhs; }

	constexpr bool operator==(const LWOSCENEID& rhs) const noexcept { return (m_sceneID == rhs.m_sceneID && m_layerID == rhs.m_layerID); }
	constexpr bool operator==(const int32_t rhs) const noexcept { return m_sceneID == rhs; }

	constexpr int32_t GetSceneID() const noexcept { return m_sceneID; }
	constexpr eSceneType GetLayerID() const noexcept { return m_layerID; }

	constexpr void SetSceneID(const int32_t sceneID) noexcept { m_sceneID = sceneID; }
	constexpr void SetLayerID(const eSceneType layerID) noexcept { m_layerID = layerID; }

private:
	int32_t m_sceneID;
	eSceneType m_layerID;
};

constexpr LWOSCENEID LWOSCENEID_INVALID = -1;

#endif //!LWOSCENEID_H
