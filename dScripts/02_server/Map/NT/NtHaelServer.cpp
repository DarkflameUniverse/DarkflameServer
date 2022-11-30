#include "NtHaelServer.h"
#include "Entity.h"

void NtHaelServer::SetVariables(Entity* self) {
	self->SetVar<float_t>(m_SpyProximityVariable, 25.0f);

	self->SetVar<SpyData>(m_SpyDataVariable, {
			NT_FACTION_SPY_HAEL, 13892, 1321
		});

	self->SetVar<std::vector<SpyDialogue>>(m_SpyDialogueTableVariable, {
			{ "HAEL_NT_CONVO_1", 0 },
			{ "HAEL_NT_CONVO_2", 0 },
			{ "HAEL_NT_CONVO_3", 0 },
			{ "HAEL_NT_CONVO_4", 0 },
		});

	// If there's an alternating conversation, indices should be provided using the conversationID variables
	self->SetVar<std::vector<LWOOBJID>>(m_SpyCinematicObjectsVariable, { self->GetObjectID() });
}
