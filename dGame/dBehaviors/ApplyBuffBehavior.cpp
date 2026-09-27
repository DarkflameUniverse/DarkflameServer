#include "ApplyBuffBehavior.h"
#include "EntityManager.h"
#include "BehaviorContext.h"
#include "BehaviorBranchContext.h"
#include "BuffComponent.h"


void ApplyBuffBehavior::Handle(BehaviorContext* context, RakNet::BitStream& bitStream, BehaviorBranchContext branch) {
	auto* entity = Game::entityManager->GetEntity(branch.target == LWOOBJID_EMPTY ? context->originator : branch.target);

	if (entity == nullptr) return;

	auto* buffComponent = entity->GetComponent<BuffComponent>();

	if (buffComponent == nullptr) return;

	buffComponent->ApplyBuff(m_BuffId, m_Duration, context->originator, addImmunity, cancelOnDamaged, cancelOnDeath,
		cancelOnLogout, cancelonRemoveBuff, cancelOnUi, cancelOnUnequip, cancelOnZone, m_ApplyOnTeammates);
}

void ApplyBuffBehavior::UnCast(BehaviorContext* context, BehaviorBranchContext branch) {
	auto* entity = Game::entityManager->GetEntity(branch.target);

	if (entity == nullptr) return;

	auto* buffComponent = entity->GetComponent<BuffComponent>();

	if (buffComponent == nullptr) return;

	// UnCast only runs when the item (or item set) that cast the buff is unequipped. Live servers sent those
	// removals with bFromUnEquip set (2014 captures: buffs 3, 4, 5, 50 and 61, all cancel_on_unequip).
	buffComponent->RemoveBuff(m_BuffId, true);
}

void ApplyBuffBehavior::Calculate(BehaviorContext* context, RakNet::BitStream& bitStream, BehaviorBranchContext branch) {
	Handle(context, bitStream, branch);
}

void ApplyBuffBehavior::Load() {
	m_BuffId = GetInt("buff_id");
	m_Duration = GetFloat("duration_secs");
	addImmunity = GetBoolean("add_immunity");
	cancelOnDamaged = GetBoolean("cancel_on_damaged");
	cancelOnDeath = GetBoolean("cancel_on_death");
	cancelOnLogout = GetBoolean("cancel_on_logout");
	cancelonRemoveBuff = GetBoolean("cancel_on_remove_buff");
	cancelOnUi = GetBoolean("cancel_on_ui");
	cancelOnUnequip = GetBoolean("cancel_on_unequip");
	cancelOnZone = GetBoolean("cancel_on_zone");
	m_ApplyOnTeammates = GetBoolean("apply_on_teammates");
}
