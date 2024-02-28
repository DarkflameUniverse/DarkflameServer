#pragma once

#include "Component.h"
#include "Entity.h"
#include "eReplicaComponentType.h"
#include <cstdint>
#include <array>
#include "dCommonVars.h"

/**
 * Component that handles the LOT that is shown in the LUP exhibit in the LUP world. Works by setting a timer and
 * switching the LOTs around that we'd like to display.
 */
class LUPExhibitComponent final : public Component
{
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::LUP_EXHIBIT;

	LUPExhibitComponent(Entity* parent) : Component(parent) {};
	void Update(float deltaTime) override;
	void Serialize(RakNet::BitStream& outBitStream, bool bIsInitialUpdate) override;
	void NextLUPExhibit();
private:
	float m_UpdateTimer = 0.0f;
	std::array<LOT, 4> m_LUPExhibits = { 11121, 11295, 11423, 11979 };
	uint8_t m_LUPExhibitIndex = 0;
	bool m_DirtyLUPExhibit = true;
};
