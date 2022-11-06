#include "BuffBehavior.h"

#include "BehaviorContext.h"
#include "BehaviorBranchContext.h"
#include "EntityManager.h"
#include "Game.h"
#include "dLogger.h"
#include "DestroyableComponent.h"

void BuffBehavior::Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	const auto target = branch.target != LWOOBJID_EMPTY ? branch.target : context->originator;

	auto* entity = EntityManager::Instance()->GetEntity(target);

	if (entity == nullptr) {
		Game::logger->Log("BuffBehavior", "Invalid target (%llu)!", target);

		return;
	}

	auto* component = entity->GetComponent<DestroyableComponent>();

	if (component == nullptr) {
		Game::logger->Log("BuffBehavior", "Invalid target, no destroyable component (%llu)!", target);

		return;
	}

	component->SetMaxHealth(component->GetMaxHealth() + this->m_health);
	component->SetMaxArmor(component->GetMaxArmor() + this->m_armor);
	component->SetMaxImagination(component->GetMaxImagination() + this->m_imagination);

	EntityManager::Instance()->SerializeEntity(entity);

	if (!context->unmanaged) {
		if (branch.duration > 0) {
			context->RegisterTimerBehavior(this, branch);
		} else if (branch.start > 0) {
			context->RegisterEndBehavior(this, branch);
		}
	}
}

void BuffBehavior::UnCast(BehaviorContext* context, BehaviorBranchContext branch) {
	const auto target = branch.target != LWOOBJID_EMPTY ? branch.target : context->originator;

	auto* entity = EntityManager::Instance()->GetEntity(target);

	if (entity == nullptr) {
		Game::logger->Log("BuffBehavior", "Invalid target (%llu)!", target);

		return;
	}

	auto* component = entity->GetComponent<DestroyableComponent>();

	if (component == nullptr) {
		Game::logger->Log("BuffBehavior", "Invalid target, no destroyable component (%llu)!", target);

		return;
	}

	component->SetMaxHealth(component->GetMaxHealth() - this->m_health);
	component->SetMaxArmor(component->GetMaxArmor() - this->m_armor);
	component->SetMaxImagination(component->GetMaxImagination() - this->m_imagination);

	EntityManager::Instance()->SerializeEntity(entity);
}

void BuffBehavior::Timer(BehaviorContext* context, const BehaviorBranchContext branch, LWOOBJID second) {
	UnCast(context, branch);
}

void BuffBehavior::End(BehaviorContext* context, const BehaviorBranchContext branch, LWOOBJID second) {
	UnCast(context, branch);
}

void BuffBehavior::Load() {
	this->m_health = GetInt("life");

	this->m_armor = GetInt("armor");

	this->m_imagination = GetInt("imag");
}
