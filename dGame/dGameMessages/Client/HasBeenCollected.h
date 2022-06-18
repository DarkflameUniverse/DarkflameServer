#pragma once

#include "../GameMessage.h"
#include "MissionComponent.h"
#include "EntityManager.h"

class HasBeenCollected : public GameMessage {
public:
	constexpr static GAME_MSG GetId() { return GAME_MSG_HAS_BEEN_COLLECTED; }

	LWOOBJID playerID;

	void Deserialize(RakNet::BitStream* inStream) override {
		inStream->Read(playerID);
	};

	void Handle() override {
		Entity* player = EntityManager::Instance()->GetEntity(playerID);
		if (!player || !associate || associate->GetCollectibleID() == 0) return;

		MissionComponent* missionComponent = static_cast<MissionComponent*>(player->GetComponent(COMPONENT_TYPE_MISSION));
		if (missionComponent) {
			missionComponent->Progress(MissionTaskType::MISSION_TASK_TYPE_ENVIRONMENT, associate->GetLOT(), associate->GetObjectID());
		}
	}
};