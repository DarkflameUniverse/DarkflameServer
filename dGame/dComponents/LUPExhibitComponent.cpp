#include "LUPExhibitComponent.h"

#include "EntityManager.h"

LUPExhibitComponent::LUPExhibitComponent(Entity* parent) : Component(parent) {
	m_Exhibits = { 11121, 11295, 11423, 11979 };

	m_ExhibitIndex = 0;

	m_Exhibit = m_Exhibits[m_ExhibitIndex];


}

LUPExhibitComponent::~LUPExhibitComponent() {

}

void LUPExhibitComponent::Update(float deltaTime) {
	m_UpdateTimer += deltaTime;

	if (m_UpdateTimer > 20.0f) {
		NextExhibit();

		m_UpdateTimer = 0.0f;
	}
}

void LUPExhibitComponent::NextExhibit() {
	m_ExhibitIndex++;

	if (m_ExhibitIndex >= m_Exhibits.size()) {
		m_ExhibitIndex = 0;
	}

	m_Exhibit = m_Exhibits[m_ExhibitIndex];

	EntityManager::Instance()->SerializeEntity(m_Parent);
}

void LUPExhibitComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, uint32_t& flags) {
	outBitStream->Write1(); // Dirty flag?
	outBitStream->Write(m_Exhibit);
}
