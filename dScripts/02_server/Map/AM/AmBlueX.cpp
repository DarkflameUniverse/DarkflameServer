#include "AmBlueX.h"
#include "SkillComponent.h"
#include "EntityManager.h"
#include "Character.h"

void AmBlueX::OnUse(Entity* self, Entity* user) {
	auto* skillComponent = user->GetComponent<SkillComponent>();
	if (skillComponent != nullptr) {
		skillComponent->CalculateBehavior(m_SwordSkill, m_SwordBehavior, self->GetObjectID());
	}
}

void AmBlueX::OnSkillEventFired(Entity* self, Entity* caster, const std::string& message) {
	if (message == "FireDukesStrike") {
		self->SetNetworkVar<bool>(m_XUsedVariable, true);
		self->SetNetworkVar<bool>(m_StartEffectVariable, true);

		auto* character = caster->GetCharacter();
		if (character != nullptr) {
			character->SetPlayerFlag(self->GetVar<int32_t>(m_FlagVariable), true);
		}

		EntityInfo info{};
		info.lot = m_FXObject;
		info.pos = self->GetPosition();
		info.rot = self->GetRotation();
		info.spawnerID = self->GetObjectID();

		auto* fxObject = EntityManager::Instance()->CreateEntity(info, nullptr, self);
		EntityManager::Instance()->ConstructEntity(fxObject);

		auto fxObjectID = fxObject->GetObjectID();
		auto playerID = caster->GetObjectID();

		// Add a callback for the bomb to explode
		self->AddCallbackTimer(m_BombTime, [this, self, fxObjectID, playerID]() {
			auto* fxObject = EntityManager::Instance()->GetEntity(fxObjectID);
			auto* player = EntityManager::Instance()->GetEntity(playerID);
			auto* skillComponent = self->GetComponent<SkillComponent>();

			if (skillComponent == nullptr)
				return;

			// Cast the skill that destroys the object
			if (player != nullptr) {
				skillComponent->CalculateBehavior(m_AOESkill, m_AOEBehavior, LWOOBJID_EMPTY, false, false, playerID);
			} else {
				skillComponent->CalculateBehavior(m_AOESkill, m_AOEBehavior, LWOOBJID_EMPTY);
			}

			fxObject->Smash();
			self->Smash();
			});
	}
}
