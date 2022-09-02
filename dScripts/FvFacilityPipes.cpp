#include "FvFacilityPipes.h"
#include "GameMessages.h"

void FvFacilityPipes::OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2, int32_t param3) {
	if (args == "startFX") {
		GameMessages::SendPlayFXEffect(self->GetObjectID(), m_LeftPipeEffectID, m_EffectType, m_LeftPipeEffectName);
		GameMessages::SendPlayFXEffect(self->GetObjectID(), m_RightPipeEffectID, m_EffectType, m_RightPipeEffectName);
		GameMessages::SendPlayFXEffect(self->GetObjectID(), m_ImaginationCanisterEffectID, m_EffectType, m_ImaginationCanisterEffectName);
	}
}
