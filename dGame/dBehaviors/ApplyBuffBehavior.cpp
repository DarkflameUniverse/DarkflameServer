#include "ApplyBuffBehavior.h"
#include "EntityManager.h"
#include "BehaviorContext.h"
#include "BehaviorBranchContext.h"
#include "BuffComponent.h"


void ApplyBuffBehavior::Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	branch.target == LWOOBJID_EMPTY ? context->originator : branch.target;
	if (m_TargetCaster) branch.target = context->originator;
	auto* entity = EntityManager::Instance()->GetEntity(branch.target);

	if (entity == nullptr) return;

	auto* buffComponent = entity->GetComponent<BuffComponent>();

	if (buffComponent == nullptr) return;

	buffComponent->ApplyBuff(m_BuffId, m_Duration, context->originator, m_AddImmunity, m_ApplyOnTeammates m_CancelOnDamaged, m_CancelOnDeath,
		m_CancelOnLogout, m_CancelOnRemoveBuff, m_CancelOnUi, m_CancelOnUnequip, m_CancelOnZone, m_CancelOnDamageAbsDone, m_UseRefCount);
}

void ApplyBuffBehavior::UnCast(BehaviorContext* context, BehaviorBranchContext branch) {
	if (m_IgnoreUncast) return;
	auto* entity = EntityManager::Instance()->GetEntity(branch.target);

	if (entity == nullptr) return;

	auto* buffComponent = entity->GetComponent<BuffComponent>();

	if (buffComponent == nullptr) return;

	buffComponent->RemoveBuff(m_BuffId);
}

void ApplyBuffBehavior::Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	Handle(context, bitStream, branch);
}

void ApplyBuffBehavior::Load() {
	m_BuffId = GetInt("buff_id");
	m_Duration = GetFloat("duration_secs");
	m_IgnoreUncast = GetBoolean("ignore_uncast", false);
	m_TargetCaster = GetBoolean("target_caster", false);
	m_AddImmunity = GetBoolean("add_immunity", false);
	m_ApplyOnTeammates = GetBoolean("apply_on_teammates", false);
	m_CancelOnDamaged = GetBoolean("cancel_on_damaged", false);
	m_CancelOnDeath = GetBoolean("cancel_on_death", false);
	m_CancelOnLogout = GetBoolean("cancel_on_logout", false);
	m_CancelOnRemoveBuff = GetBoolean("cancel_on_remove_buff", false);
	m_CancelOnUi = GetBoolean("cancel_on_ui", false);
	m_CancelOnUnequip = GetBoolean("cancel_on_unequip", false);
	m_CancelOnZone = GetBoolean("cancel_on_zone", false);
	m_CancelOnDamageAbsDone = GetBoolean("cancel_on_damage_abs_done", false);
	m_UseRefCount =  GetBoolean("use_ref_count", false);
}
