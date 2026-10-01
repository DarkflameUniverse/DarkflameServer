#include "PossessableComponent.h"
#include "ObjectMessages.h"
#include "PossessorComponent.h"
#include "EntityManager.h"
#include "Inventory.h"
#include "Item.h"

PossessableComponent::PossessableComponent(Entity* parent, const int32_t componentID) : Component(parent, componentID) {
	RegisterMsg(&PossessableComponent::OnRequestUse);
	m_Possessor = LWOOBJID_EMPTY;
}

// The item's animation flag and the PossessableComponent row
void PossessableComponent::LoadDataFromTemplate() {
	CDItemComponent item = Inventory::FindItemComponent(m_Parent->GetLOT());
	m_AnimationFlag = static_cast<eAnimationFlags>(item.animationFlag);

	// Get the possession Type from the CDClient
	auto query = CDClientDatabase::CreatePreppedStmt("SELECT possessionType, depossessOnHit, skillSet FROM PossessableComponent WHERE id = ?;");

	query.bind(1, static_cast<int>(m_ComponentID));

	auto result = query.execQuery();

	// Should a result not exist for this default to attached visible
	if (!result.eof()) {
		m_PossessionType = static_cast<ePossessionType>(result.getIntField("possessionType", 1)); // Default to Attached Visible
		m_DepossessOnHit = static_cast<bool>(result.getIntField("depossessOnHit", 0));
		m_SkillSet = result.getIntField("skillSet", 0);
	} else {
		m_PossessionType = ePossessionType::ATTACHED_VISIBLE;
		m_DepossessOnHit = false;
	}
	result.finalize();
}

void PossessableComponent::Serialize(RakNet::BitStream& outBitStream, bool bIsInitialUpdate) {
	outBitStream.Write(m_DirtyPossessable || bIsInitialUpdate);
	if (m_DirtyPossessable || bIsInitialUpdate) {
		m_DirtyPossessable = false; // reset flag
		outBitStream.Write(m_Possessor != LWOOBJID_EMPTY);
		if (m_Possessor != LWOOBJID_EMPTY) outBitStream.Write(m_Possessor);

		outBitStream.Write(m_AnimationFlag != eAnimationFlags::IDLE_NONE);
		if (m_AnimationFlag != eAnimationFlags::IDLE_NONE) outBitStream.Write(m_AnimationFlag);

		outBitStream.Write(m_ImmediatelyDepossess);
		m_ImmediatelyDepossess = false; // reset flag
	}
}

void PossessableComponent::Dismount() {
	SetPossessor(LWOOBJID_EMPTY);
	if (m_ItemSpawned) m_Parent->ScheduleKillAfterUpdate();
}

bool PossessableComponent::OnRequestUse(GameMessages::RequestUseEvent& event) {
	if (event.msg.IsMissionChoice()) return false;
	auto* const originator = event.user;
	auto* possessor = originator->GetComponent<PossessorComponent>();
	if (possessor) {
		possessor->Mount(m_Parent);
	}
	return true;
}
