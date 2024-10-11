#include "NjRailActivatorsServer.h"
#include "QuickBuildComponent.h"
#include "SkillComponent.h"
#include "Character.h"

//	Temp place to store DartSpinner Fire Activator

void NjRailActivatorsServer::OnUse(Entity* self, Entity* user) {
	const auto flag = self->GetVar<int32_t>(u"RailFlagNum");
	auto* quickBuildComponent = self->GetComponent<QuickBuildComponent>();
	
	// Only allow use if this is not a quick build or the quick build is built
	if (quickBuildComponent == nullptr || quickBuildComponent->GetState() == eQuickBuildState::COMPLETED) {
		auto* character = user->GetCharacter();
		if (character != nullptr) {
			character->SetPlayerFlag(flag, true);
		}
	}
	
	if (quickBuildComponent != nullptr && quickBuildComponent->GetState() != eQuickBuildState::COMPLETED &&
	self->GetVar<int>(u"SpinnerIsUp") == 1) {		


		self->SetVar<LWOOBJID>(u"lastUserID", user->GetObjectID());

		self->AddTimer("FailRebuild", 1);	
	}	
}

void NjRailActivatorsServer::OnQuickBuildComplete(Entity* self, Entity* target) {	
	if (self->GetVar<std::u16string>(u"RailGroup") == u"DartSpinnersFRail2") {
		const auto EndpostEntity = Game::entityManager->GetEntitiesInGroup("DartSpinnersFRail2");		
		for (auto* endpost : EndpostEntity) {
			auto* quickBuildComponent = endpost->GetComponent<QuickBuildComponent>();	
			if (quickBuildComponent == nullptr || quickBuildComponent->GetState() == eQuickBuildState::COMPLETED) {
				self->SetNetworkVar<bool>(u"NetworkNotActive", false);	
				self->SetVar<bool>(u"NotActive", false);	
			}	
		}			
	}
}	

void NjRailActivatorsServer::OnTimerDone(Entity* self, std::string timerName) {
	auto* quickBuildComponent = self->GetComponent<QuickBuildComponent>();
	const auto playerID = self->GetVar<LWOOBJID>(u"lastUserID");
	auto* player = Game::entityManager->GetEntity(playerID);
	
	if (!player) return;
	if (timerName == "FailRebuild") {	
		auto* skillComponent = player->GetComponent<SkillComponent>();
		
		quickBuildComponent->ResetQuickBuild(true);
		skillComponent->CalculateBehavior(1672, 40837, playerID, true);
		
		auto dir = player->GetRotation().GetForwardVector();
		dir.y = 15;
		dir.x = -dir.x * 20;
		dir.z = -dir.z * 20;
		GameMessages::SendKnockback(player->GetObjectID(), self->GetObjectID(), self->GetObjectID(), 1000, dir);	
	}
}