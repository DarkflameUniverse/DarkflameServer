#include "LUPExhibitComponent.h"

#include "EntityManager.h"

LUPExhibitComponent::LUPExhibitComponent(Entity* parent) : Component(parent) {
	m_LUPExhibits = { 11121, 11295, 11423, 11979 };
	m_LUPExhibitIndex = 0;
	m_LUPExhibit = m_LUPExhibits[m_LUPExhibitIndex];
}

void LUPExhibitComponent::Update(float deltaTime) {
	m_UpdateTimer += deltaTime;
	if (m_UpdateTimer > 20.0f) {
		NextLUPExhibit();
		m_UpdateTimer = 0.0f;
	}
}

void LUPExhibitComponent::NextLUPExhibit() {
	m_LUPExhibitIndex++;
	if (m_LUPExhibitIndex >= m_LUPExhibits.size()) m_LUPExhibitIndex = 0;
	if (m_LUPExhibit != m_LUPExhibits[m_LUPExhibitIndex]) {
		m_LUPExhibit = m_LUPExhibits[m_LUPExhibitIndex];
		m_DirtyLUPExhibit = true;
		Game::entityManager->SerializeEntity(m_Parent);
	}
}

void LUPExhibitComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate) {
	outBitStream->Write(m_DirtyLUPExhibit);
	if (m_DirtyLUPExhibit) {
		outBitStream->Write(m_LUPExhibit);
		if (!bIsInitialUpdate) m_DirtyLUPExhibit = false;
	}
}
