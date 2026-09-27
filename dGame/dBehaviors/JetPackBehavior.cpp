#include "JetPackBehavior.h"

#include "BehaviorBranchContext.h"
#include "GameMessages.h"
#include "MovementMessages.h"

#include "Character.h"

void JetPackBehavior::Handle(BehaviorContext* context, RakNet::BitStream& bit_stream, const BehaviorBranchContext branch) {
	auto* entity = Game::entityManager->GetEntity(branch.target);
	if (!entity) return;

	GameMessages::SetJetPackMode jetPackMode;
	jetPackMode.target = entity->GetObjectID();
	jetPackMode.bUse = true;
	jetPackMode.bBypassChecks = this->m_BypassChecks;
	jetPackMode.bDoHover = this->m_EnableHover;
	jetPackMode.effectID = this->m_effectId;
	jetPackMode.fAirspeed = this->m_Airspeed;
	jetPackMode.fMaxAirspeed = this->m_MaxAirspeed;
	jetPackMode.fVertVel = this->m_VerticalVelocity;
	jetPackMode.iWarningEffectID = this->m_WarningEffectID;
	jetPackMode.Send(UNASSIGNED_SYSTEM_ADDRESS);

	if (entity->IsPlayer()) {
		auto* character = entity->GetCharacter();

		if (character) {
			character->SetIsFlying(true);
		}
	}
}

void JetPackBehavior::UnCast(BehaviorContext* context, BehaviorBranchContext branch) {
	auto* entity = Game::entityManager->GetEntity(branch.target);
	if (!entity) return;

	GameMessages::SetJetPackMode jetPackMode;
	jetPackMode.target = entity->GetObjectID();
	jetPackMode.bUse = false;
	jetPackMode.Send(UNASSIGNED_SYSTEM_ADDRESS);

	if (entity->IsPlayer()) {
        auto* character = entity->GetCharacter();

        if (character) {
            character->SetIsFlying(false);
        }
    }
}

void JetPackBehavior::Calculate(BehaviorContext* context, RakNet::BitStream& bit_stream, const BehaviorBranchContext branch) {
	Handle(context, bit_stream, branch);
}

void JetPackBehavior::Load() {
	this->m_WarningEffectID = GetInt("warning_effect_id", -1);
	this->m_Airspeed = GetFloat("airspeed", 10);
	this->m_MaxAirspeed = GetFloat("max_airspeed", 15);
	this->m_VerticalVelocity = GetFloat("vertical_velocity", 1);
	this->m_EnableHover = GetBoolean("enable_hover", false);

	// TODO: Implement proper jetpack checks, so we can set this default to false
	this->m_BypassChecks = GetBoolean("bypass_checks", true); 
}
