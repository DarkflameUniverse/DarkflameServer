#include "Darkitect.h"
#include "MissionComponent.h"
#include "DestroyableComponent.h"
#include "EntityManager.h"
#include "GameMessages.h"
#include "Character.h"

void Darkitect::Reveal(Entity* self, Entity* player) {
	const auto playerID = player->GetObjectID();

	GameMessages::SendNotifyClientObject(self->GetObjectID(), u"reveal", 0, 0, playerID, "", player->GetSystemAddress());

	self->AddCallbackTimer(20, [this, self, playerID]() {
		auto* player = EntityManager::Instance()->GetEntity(playerID);

		if (!player) return;

		auto* destroyableComponent = player->GetComponent<DestroyableComponent>();
		auto* missionComponent = player->GetComponent<MissionComponent>();
		auto* character = player->GetCharacter();

		if (destroyableComponent != nullptr && missionComponent != nullptr && character != nullptr) {
			destroyableComponent->SetArmor(0);
			destroyableComponent->SetHealth(1);
			destroyableComponent->SetImagination(0);

			if (missionComponent->GetMissionState(1295) == MissionState::MISSION_STATE_ACTIVE) {
				character->SetPlayerFlag(1911, true);
			}

			EntityManager::Instance()->SerializeEntity(player);
		}
		});
}
