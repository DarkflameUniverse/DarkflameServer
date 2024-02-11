#include "ConsumeItemBehavior.h"
#include "BehaviorContext.h"
#include "BehaviorBranchContext.h"
#include "InventoryComponent.h"

void ConsumeItemBehavior::Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	auto action_to_cast = m_ActionNotConsumed;
	if (this->m_ConsumeLOT != -1) {
		auto caster = Game::entityManager->GetEntity(context->caster);
		if (!caster) return;

		auto inventoryComponent = caster->GetComponent<InventoryComponent>();
		if (!inventoryComponent) return;

		if (inventoryComponent->RemoveItem(this->m_ConsumeLOT, this->m_NumToConsume, eInventoryType::INVALID, false, true)){
			action_to_cast = m_ActionConsumed;
		}
	}
	if(action_to_cast) action_to_cast->Handle(context, bitStream, branch);
}

void ConsumeItemBehavior::Sync(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	Handle(context, bitStream, branch);
}

void ConsumeItemBehavior::Load() {
	this->m_ConsumeLOT = GetInt("consume_lot", -1);
	this->m_NumToConsume = GetInt("num_to_consume", 1);
	this->m_ActionNotConsumed = GetAction("action_not_consumed");
	this->m_ActionConsumed = GetAction("action_consumed");
}
