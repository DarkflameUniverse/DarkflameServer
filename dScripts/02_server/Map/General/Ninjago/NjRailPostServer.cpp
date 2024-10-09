#include "NjRailPostServer.h"
#include "QuickBuildComponent.h"
#include "SkillComponent.h"
#include "EntityManager.h"

void NjRailPostServer::OnStartup(Entity* self) {
	auto* quickBuildComponent = self->GetComponent<QuickBuildComponent>();
	if (quickBuildComponent != nullptr) {
		self->SetNetworkVar<bool>(NetworkNotActiveVariable, true);
	}
}

void NjRailPostServer::OnNotifyObject(Entity* self, Entity* sender, const std::string& name, int32_t param1,
	int32_t param2) {
	if (name == "PostRebuilt") {
		self->SetNetworkVar<bool>(NetworkNotActiveVariable, false);
	} else if (name == "PostDied") {
		self->SetNetworkVar<bool>(NetworkNotActiveVariable, true);
	}
}

void NjRailPostServer::OnQuickBuildNotifyState(Entity* self, eQuickBuildState state) {
	if (state == eQuickBuildState::COMPLETED) {
		auto* relatedRail = GetRelatedRail(self);
		if (relatedRail == nullptr)
			return;

		relatedRail->NotifyObject(self, "PostRebuilt");

		if (self->GetVar<bool>(NotActiveVariable))
			return;

		self->SetNetworkVar(NetworkNotActiveVariable, false);
	} else if (state == eQuickBuildState::RESETTING) {
		auto* relatedRail = GetRelatedRail(self);
		if (relatedRail == nullptr)
			return;

		relatedRail->NotifyObject(self, "PostDied");
	}
}

void NjRailPostServer::OnUse(Entity* self, Entity* user) {
	auto* quickBuildComponent = self->GetComponent<QuickBuildComponent>();

	if (quickBuildComponent != nullptr && quickBuildComponent->GetState() != eQuickBuildState::COMPLETED &&
	self->GetVar<int>(u"SpinnerIsUp") == 1) {		


		self->SetVar<LWOOBJID>(u"lastUserID", user->GetObjectID());

		self->AddTimer("FailRebuild", 1);	
	}	

}

void NjRailPostServer::OnTimerDone(Entity* self, std::string timerName) {
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

Entity* NjRailPostServer::GetRelatedRail(Entity* self) {
	const auto& railGroup = self->GetVar<std::u16string>(RailGroupVariable);
	if (!railGroup.empty()) {
		for (auto* entity : Game::entityManager->GetEntitiesInGroup(GeneralUtils::UTF16ToWTF8(railGroup))) {
			return entity;
		}
	}

	return nullptr;
}
