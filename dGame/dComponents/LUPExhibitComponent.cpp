#include "LUPExhibitComponent.h"
#include "EntityManager.h"

void LUPExhibitComponent::Update(float deltaTime) {
	m_UpdateTimer += deltaTime;
	if (m_UpdateTimer > 20.0f) {
		NextLUPExhibit();
		m_UpdateTimer = 0.0f;
	}
}

void LUPExhibitComponent::NextLUPExhibit() {
	m_LUPExhibitIndex++;
	m_DirtyLUPExhibit = true;
	Game::entityManager->SerializeEntity(m_Parent);
}

void LUPExhibitComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate) {
	outBitStream->Write(m_DirtyLUPExhibit);
	if (m_DirtyLUPExhibit) {
		outBitStream->Write(m_LUPExhibits[m_LUPExhibitIndex % m_LUPExhibits.size()]);
		if (!bIsInitialUpdate) m_DirtyLUPExhibit = false;
	}
}
