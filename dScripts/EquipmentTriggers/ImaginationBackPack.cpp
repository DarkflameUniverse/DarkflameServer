#include "ImaginationBackPack.h"

#include "SkillComponent.h"

void ImaginationBackPack::OnFactionTriggerItemEquipped(Entity* itemOwner, LWOOBJID itemObjId) {
	LOG("Subscribing to PlayerResurrectionFinished");
	itemOwner->Subscribe(itemObjId, this, "PlayerResurrectionFinished");
}

void ImaginationBackPack::NotifyPlayerResurrectionFinished(Entity& self, GameMessages::PlayerResurrectionFinished& msg) {
	LOG("PlayerResurrectionFinished");
	auto* skillComponent = self.GetComponent<SkillComponent>();
	if (!skillComponent) return;
	LOG("Casting skill 1334");
	skillComponent->CastSkill(1334);
}

void ImaginationBackPack::OnFactionTriggerItemUnequipped(Entity* itemOwner, LWOOBJID itemObjId) {
	LOG("Unsubscribing from PlayerResurrectionFinished");
	itemOwner->Unsubscribe(itemObjId, "PlayerResurrectionFinished");
}

