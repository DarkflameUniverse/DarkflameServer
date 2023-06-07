#include "RemoveBuffBehavior.h"

#include "BehaviorBranchContext.h"
#include "BehaviorContext.h"
#include "EntityManager.h"
#include "BuffComponent.h"

void RemoveBuffBehavior::Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	auto* entity = EntityManager::Instance()->GetEntity(context->caster);
	if (!entity) return;

	auto buffComponent = entity->GetComponent<BuffComponent>();
	if (!buffComponent) return;

	buffComponent->RemoveBuff(m_BuffId, false, m_RemoveImmunity);
}

void RemoveBuffBehavior::Load() {
	this->m_RemoveImmunity = GetBoolean("remove_immunity");
	this->m_BuffId = GetInt("buff_id");
}
