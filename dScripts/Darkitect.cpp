#include "Darkitect.h"

#include "MissionComponent.h"
#include "DestroyableComponent.h"
#include "EntityManager.h"
#include "GameMessages.h"
#include "eMissionState.h"

void Darkitect::Reveal(Entity* self, Entity* player) {
	const auto playerID = player->GetObjectID();

	GameMessages::SendNotifyClientObject(self->GetObjectID(), u"reveal", 0, 0, playerID, "", player->GetSystemAddress());

	self->AddCallbackTimer(20, [this, self, playerID]() {
		auto* player = Game::entityManager->GetEntity(playerID);

		if (!player) return;

		auto* destroyableComponent = player->GetComponent<DestroyableComponent>();
		auto* missionComponent = player->GetComponent<MissionComponent>();

		if (destroyableComponent != nullptr && missionComponent != nullptr) {
			destroyableComponent->SetArmor(0);
			destroyableComponent->SetHealth(1);
			destroyableComponent->SetImagination(0);

			if (missionComponent->GetMissionState(1295) == eMissionState::ACTIVE) {
				GameMessages::SetFlag setFlag{};
				setFlag.target = player->GetObjectID();
				setFlag.iFlagId = 1911;
				setFlag.bFlag = true;
				SEND_ENTITY_MSG(setFlag);
			}

			Game::entityManager->SerializeEntity(player);
		}
		});
}
