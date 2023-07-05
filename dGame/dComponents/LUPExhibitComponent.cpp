#include "LUPExhibitComponent.h"

#include "EntityManager.h"

LUPExhibitComponent::LUPExhibitComponent(Entity* parent) : Component(parent) {
	m_ExhibitIndex = 0;
	m_UpdateTimer = 0.0f;
	m_Exhibit = m_Exhibits.front();
	m_DirtyExhibitInfo = true;
}

void LUPExhibitComponent::Update(float deltaTime) {
	m_UpdateTimer += deltaTime;
	if (m_UpdateTimer < 20.0f) return;

	NextExhibit();
	m_UpdateTimer = 0.0f;
}

void LUPExhibitComponent::NextExhibit() {
	m_ExhibitIndex++;

	// After 1361 years, this will skip exhibit 4 one time.  I think modulo is ok here.
	m_Exhibit = m_Exhibits.at(m_ExhibitIndex % m_Exhibits.size());
	EntityManager::Instance()->SerializeEntity(m_ParentEntity);
}

void LUPExhibitComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, uint32_t& flags) {
	outBitStream->Write(bIsInitialUpdate || m_DirtyExhibitInfo);
	if (bIsInitialUpdate || m_DirtyExhibitInfo) {
		outBitStream->Write(m_Exhibit);
		if (!bIsInitialUpdate) m_DirtyExhibitInfo = false;
	}
}
