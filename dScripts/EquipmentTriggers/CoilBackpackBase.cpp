#include "CoilBackpackBase.h"

#include "Entity.h"
#include "SkillComponent.h"

void CoilBackpackBase::OnFactionTriggerItemEquipped(Entity* itemOwner, LWOOBJID itemObjId) {
	itemOwner->Subscribe(itemObjId, this);
	itemOwner->SetVar<uint8_t>(u"coilCount", 0);
}

void CoilBackpackBase::OnHitOrHealResult(Entity* self, Entity* attacker, int32_t damage) {
	if (damage > 0) {
		self->SetVar<uint8_t>(u"coilCount", self->GetVar<uint8_t>(u"coilCount") + 1);
		if (self->GetVar<uint8_t>(u"coilCount") > 4) {
			auto* skillComponent = self->GetComponent<SkillComponent>();
			if (!skillComponent) return;
			skillComponent->CalculateBehavior(m_SkillId, m_BehaviorId, self->GetObjectID());
			self->SetVar<uint8_t>(u"coilCount", 0);
		}
	}	
}

void CoilBackpackBase::OnFactionTriggerItemUnequipped(Entity* itemOwner, LWOOBJID itemObjId) {
	itemOwner->Unsubscribe(itemObjId);
}
