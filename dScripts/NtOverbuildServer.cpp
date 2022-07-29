#include "NtOverbuildServer.h"
#include "EntityManager.h"

void NtOverbuildServer::SetVariables(Entity* self) {
	self->SetVar<float_t>(m_SpyProximityVariable, 30.0f);

	self->SetVar<SpyData>(m_SpyDataVariable, {
			NT_FACTION_SPY_OVERBUILD, 13891, 1320
		});

	self->SetVar<std::vector<SpyDialogue>>(m_SpyDialogueTableVariable, {
			{ "OVERBUILD_NT_CONVO_1", 0 },
			{ "OVERBUILD_NT_CONVO_2", 1 },
			{ "OVERBUILD_NT_CONVO_3", 0 },
			{ "OVERBUILD_NT_CONVO_4", 1 },
			{ "OVERBUILD_NT_CONVO_5", 0 },
			{ "OVERBUILD_NT_CONVO_6", 1 },
			{ "OVERBUILD_NT_CONVO_7", 0 },
		});

	// Find the second object Dr. Overbuild interacts with
	LWOOBJID otherConvoObjectID = LWOOBJID_EMPTY;
	for (auto* otherConvoObject : EntityManager::Instance()->GetEntitiesInGroup(GeneralUtils::UTF16ToWTF8(self->GetVar<std::u16string>(m_OtherEntitiesGroupVariable)))) {
		otherConvoObjectID = otherConvoObject->GetObjectID();
		break;
	}

	// If there's an alternating conversation, indices should be provided using the conversationID variables
	self->SetVar<std::vector<LWOOBJID>>(m_SpyCinematicObjectsVariable, { self->GetObjectID(), otherConvoObjectID });
}
