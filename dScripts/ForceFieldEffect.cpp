#include "ForceFieldEffect.h"
#include "GameMessages.h"

void ForceFieldEffect::OnCollisionPhantom(Entity* self, Entity* target) {
	GameMessages::SendPlayFXEffect(target->GetObjectID(), m_EffectID, m_EffectType, m_EffectName);
}

