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

	Buff buff;
	buff.id = m_BuffId;
	buff.duration = m_Duration;
	buff.source = context->originator;
	buff.addImmunity = m_AddImmunity;
	buff.applyOnTeammates = m_ApplyOnTeammates;
	buff.cancelOnDamaged = m_CancelOnDamaged;
	buff.cancelOnDeath = m_CancelOnDeath;
	buff.cancelOnLogout = m_CancelOnLogout;
	buff.cancelOnRemoveBuff = m_CancelOnRemoveBuff;
	buff.cancelOnUi = m_CancelOnUi;
	buff.cancelOnUnequip = m_CancelOnUnequip;
	buff.cancelOnZone = m_CancelOnZone;
	buff.useRefCount = m_UseRefCount;
	buff.cancelOnDamageAbsDone = m_CancelOnDamageAbsDone;

	buffComponent->ApplyBuff(buff);
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
