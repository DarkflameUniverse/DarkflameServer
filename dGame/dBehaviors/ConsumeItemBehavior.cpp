#include "ConsumeItemBehavior.h"
#include "BehaviorContext.h"
#include "BehaviorBranchContext.h"
#include "EntityManager.h"
#include "InventoryComponent.h"

void ConsumeItemBehavior::Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	auto action_to_cast = m_ActionNotConsumed;
	if (this->m_ConsumeLOT != -1) {
		auto target = EntityManager::Instance()->GetEntity(branch.target);
		if (target) {
			auto inventoryComponent = target->GetComponent<InventoryComponent>();
			if (!inventoryComponent) {
				auto count = inventoryComponent->GetLotCount(this->m_ConsumeLOT);
				if (count >= this->m_NumToConsume){
					// how do we know the removal was successful?
					inventoryComponent->RemoveItem(this->m_ConsumeLOT, this->m_NumToConsume);
					action_to_cast = m_ActionConsumed;
				}
			}
		}
	}

	if(action_to_cast) action_to_cast->Handle(context, bitStream, branch);
}

void ConsumeItemBehavior::Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	Handle(context, bitStream, branch);
}

void ConsumeItemBehavior::Load() {
	this->m_ConsumeLOT = GetInt("consume_lot", -1);
	this->m_NumToConsume = GetInt("num_to_consume", 1);
	this->m_ActionNotConsumed = GetAction("action_not_consumed");
	this->m_ActionConsumed = GetAction("action_consumed");
}
