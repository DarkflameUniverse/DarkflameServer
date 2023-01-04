#include "BubbleStatue.h"

#include "DestroyableComponent.h"
#include "Entity.h"
#include "GameMessages.h"
#include "SkillComponent.h"

void BubbleStatue::OnStartup(Entity* self) {
	self->SetProximityRadius(m_BubbleStatueRadius, "BUBBLE_STATUE_RADIUS");
	self->SetVar<bool>(u"StatueEnabled", true);
}

void BubbleStatue::OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) {
	auto* destroyableComponent = entering->GetComponent<DestroyableComponent>();
	if (!destroyableComponent) return;

	// Player faction is 1
	if (name == "BUBBLE_STATUE_RADIUS" && status == "ENTER" && !self->GetVar<bool>(u"StatueEnabled") && destroyableComponent->HasFaction(1)) {
		auto* skillComponent = self->GetComponent<SkillComponent>();
		if (!skillComponent) return;
		skillComponent->CalculateBehavior(116, 252, entering->GetObjectID());
		GameMessages::SendActivateBubbleBuffFromServer(entering->GetObjectID(), specialAnims, wszType, entering->GetSystemAddress());
	}
}

void BubbleStatue::OnNotifyObject(Entity* self, Entity* sender, const std::string& name, int32_t param1, int32_t param2) {
	if (name == "zone_state_change") {
		// CONSTANTS["ZONE_STATE_NO_INVASION"] is zero.
		// Main script is unimplemented at this time so this cannot be tested for logic.
		self->SetVar<bool>(u"StatueEnabled", param1 == 0);
	}
}
