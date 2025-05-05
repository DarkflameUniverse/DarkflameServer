#include "TrialFactionArmorServer.h"

#include "Character.h"
#include "ePlayerFlag.h"
#include "DestroyableComponent.h"

void TrialFactionArmorServer::OnFactionTriggerItemEquipped(Entity* itemOwner, LWOOBJID itemObjId) {
	auto* character = itemOwner->GetCharacter();
	if (!character) return;

	auto flag = character->GetPlayerFlag(ePlayerFlag::EQUPPED_TRIAL_FACTION_GEAR);
	if (!flag) {
		character->SetPlayerFlag(ePlayerFlag::EQUPPED_TRIAL_FACTION_GEAR, true);

		// technically a TimerWithCancel but our current implementation doesnt support this.
		itemOwner->AddCallbackTimer(1.0f, [itemOwner]() {
			auto* destroyableComponent = itemOwner->GetComponent<DestroyableComponent>();
			if (!destroyableComponent) return;

			destroyableComponent->SetHealth(destroyableComponent->GetMaxHealth());
			destroyableComponent->SetArmor(destroyableComponent->GetMaxArmor());
			destroyableComponent->SetImagination(destroyableComponent->GetMaxImagination());

			Game::entityManager->SerializeEntity(itemOwner);
		});
	}
}
