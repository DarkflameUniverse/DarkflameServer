#define _USE_MATH_DEFINES
#include <cmath>
#include "KnockbackBehavior.h"
#include "BehaviorBranchContext.h"
#include "BehaviorContext.h"
#include "EntityManager.h"
#include "GameMessages.h"
#include "DestroyableComponent.h"
#include "Game.h"
#include "Logger.h"
#include "MovementAIComponent.h"
#include "dpKnockback.h"

void KnockbackBehavior::Handle(BehaviorContext* context, RakNet::BitStream& bitStream, BehaviorBranchContext branch) {
	bool unknown{};

	if (!bitStream.Read(unknown)) {
		LOG("Unable to read unknown from bitStream, aborting Handle! %i", bitStream.GetNumberOfUnreadBits());
		return;
	};

	// The bit is whether the target blocked it
	if (!unknown) KnockbackServerObject(context, branch);
}

void KnockbackBehavior::Calculate(BehaviorContext* context, RakNet::BitStream& bitStream, BehaviorBranchContext branch) {
	bool blocked = false;

	auto* target = Game::entityManager->GetEntity(branch.target);

	if (target != nullptr) {
		auto* destroyableComponent = target->GetComponent<DestroyableComponent>();

		if (destroyableComponent != nullptr) {
			// The client knocks back its own character unless this bit is set, then checks its status immunity.
			// Answer for the immunity here too, so a client that hasn't caught up (e.g. the Personal Fortress
			// that was just raised) can't knock itself back anyway.
			blocked = destroyableComponent->IsKnockbackImmune() || destroyableComponent->GetImmuneToKnockback();
		}
	}

	bitStream.Write(blocked);

	if (!blocked) KnockbackServerObject(context, branch);
}

void KnockbackBehavior::KnockbackServerObject(BehaviorContext* context, const BehaviorBranchContext& branch) const {
	// Mirrors KnockbackBehavior::Cast in the client (1.10.64 0x004efc10)
	if (m_ignoreSelf && context->caster == branch.target) return;

	// `caster` knocks back the caster, away from the target
	const auto knockedID = m_caster ? context->caster : branch.target;
	const auto sourceID = m_caster ? branch.target : context->caster;

	auto* const knocked = Game::entityManager->GetEntity(knockedID);
	if (!knocked || knocked->IsPlayer()) return;

	auto* const movementAI = knocked->GetComponent<MovementAIComponent>();
	if (!movementAI) return;

	const auto* const destroyableComponent = knocked->GetComponent<DestroyableComponent>();
	if (destroyableComponent && destroyableComponent->GetImmuneToKnockback()) return;

	NiPoint3 away;
	if (m_relative) {
		// Backwards from where the knocked object faces
		away = QuatUtils::Forward(knocked->GetRotation()) * -1.0f;
	} else {
		const auto* const source = Game::entityManager->GetEntity(sourceID);
		if (!source) return;
		away = knocked->GetPosition() - source->GetPosition();
	}

	movementAI->Knockback(dpKnockback::ComputeVector(away, m_angle, m_strength));
}

void KnockbackBehavior::Load() {
	this->m_strength = GetFloat("strength");
	this->m_angle = GetFloat("angle");
	this->m_relative = GetBoolean("relative");
	this->m_time = GetInt("time_ms");
	this->m_ignoreSelf = GetBoolean("ignore_self", true);
	this->m_caster = GetBoolean("caster");
}
