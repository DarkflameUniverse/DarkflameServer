#include "SkillEventBehavior.h"
#include "BehaviorBranchContext.h"
#include "BehaviorContext.h"
#include "EntityManager.h"
#include "CppScripts.h"

void SkillEventBehavior::Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	auto* target = EntityManager::Instance()->GetEntity(branch.target);
	auto* caster = EntityManager::Instance()->GetEntity(context->originator);

	if (caster != nullptr && target != nullptr && this->m_effectHandle != nullptr && !this->m_effectHandle->empty()) {
		for (CppScripts::Script* script : CppScripts::GetEntityScripts(target)) {
			script->OnSkillEventFired(target, caster, *this->m_effectHandle);
		}
	}
}

void
SkillEventBehavior::Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	auto* target = EntityManager::Instance()->GetEntity(branch.target);
	auto* caster = EntityManager::Instance()->GetEntity(context->originator);

	if (caster != nullptr && target != nullptr && this->m_effectHandle != nullptr && !this->m_effectHandle->empty()) {
		for (CppScripts::Script* script : CppScripts::GetEntityScripts(target)) {
			script->OnSkillEventFired(target, caster, *this->m_effectHandle);
		}
	}
}
