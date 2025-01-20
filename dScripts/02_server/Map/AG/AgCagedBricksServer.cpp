#include "AgCagedBricksServer.h"

#include "InventoryComponent.h"
#include "GameMessages.h"
#include "EntityManager.h"
#include "ePlayerFlag.h"

void AgCagedBricksServer::OnUse(Entity* self, Entity* user) {
	//Tell the client to spawn the baby spiderling:
	auto spooders = Game::entityManager->GetEntitiesInGroup("cagedSpider");
	for (auto spodder : spooders) {
		GameMessages::SendFireEventClientSide(spodder->GetObjectID(), user->GetSystemAddress(), u"toggle", LWOOBJID_EMPTY, 0, 0, user->GetObjectID());
	}

	//Set the flag & mission status:
	GameMessages::SetFlag setFlag{};
	setFlag.target = user->GetObjectID();
	setFlag.iFlagId = ePlayerFlag::CAGED_SPIDER;
	setFlag.bFlag = true;
	SEND_ENTITY_MSG(setFlag);

	//Remove the maelstrom cube:
	auto* inv = user->GetComponent<InventoryComponent>();

	if (inv) {
		inv->RemoveItem(14553, 1);
	}
}
