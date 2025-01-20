#include "TrialFactionArmorServer.h"

#include "ePlayerFlag.h"
#include "GameMessages.h"
#include "EntityManager.h"
#include "DestroyableComponent.h"

void TrialFactionArmorServer::OnFactionTriggerItemEquipped(Entity* itemOwner, LWOOBJID itemObjId) {
	GameMessages::GetFlag flag{};
	flag.target = itemOwner->GetObjectID();
	flag.iFlagId = ePlayerFlag::EQUPPED_TRIAL_FACTION_GEAR;

	if (SEND_ENTITY_MSG(flag) && !flag.bFlag) {
		GameMessages::SetFlag setFlag{};
		setFlag.iFlagId = ePlayerFlag::EQUPPED_TRIAL_FACTION_GEAR;
		setFlag.bFlag = true;
		SEND_ENTITY_MSG(setFlag);

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
