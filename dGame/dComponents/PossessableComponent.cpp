#include "PossessableComponent.h"
#include "PossessorComponent.h"
#include "EntityManager.h"
#include "Item.h"

PossessableComponent::PossessableComponent(Entity* parent, uint32_t componentId) : Component(parent){
	m_Possessor = LWOOBJID_EMPTY;

	// Get the imagination drain rate from the CDClient
	auto query = CDClientDatabase::CreatePreppedStmt("SELECT possessionType FROM PossessableComponent WHERE id = ?;");

	query.bind(1, static_cast<int>(componentId));

	auto result = query.execQuery();

	// Should a result not exist for this pet default to 60 seconds.
	if (!result.eof() && !result.fieldIsNull(0)) {
		m_PossessionType = static_cast<ePossessionType>(result.getIntField(0, 0));
	} else {
		m_PossessionType = ePossessionType::NO_POSSESSION;
	}
	result.finalize();
}

void PossessableComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags) {
	outBitStream->Write(m_DirtyPossessable || bIsInitialUpdate);
	if (m_DirtyPossessable || bIsInitialUpdate) {
		m_DirtyPossessable = false;
		outBitStream->Write(m_Possessor != LWOOBJID_EMPTY);
		if (m_Possessor != LWOOBJID_EMPTY) outBitStream->Write(m_Possessor);

		outBitStream->Write(m_AnimationFlag != eAnimationFlags::IDLE_INVALID);
		if(m_AnimationFlag != eAnimationFlags::IDLE_INVALID) outBitStream->Write(m_AnimationFlag);

		outBitStream->Write(m_ImmediatelyDepossess);
	}
}

void PossessableComponent::OnUse(Entity* originator) {
	// TODO: rewrite
}
