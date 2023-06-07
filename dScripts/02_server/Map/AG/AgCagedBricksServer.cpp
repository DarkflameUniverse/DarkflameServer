#include "AgCagedBricksServer.h"
#include "InventoryComponent.h"
#include "GameMessages.h"
#include "Character.h"
#include "EntityManager.h"
#include "eReplicaComponentType.h"
#include "ePlayerFlag.h"

void AgCagedBricksServer::OnUse(Entity* self, Entity* user) {
	//Tell the client to spawn the baby spiderling:
	auto spooders = EntityManager::Instance()->GetEntitiesInGroup("cagedSpider");
	for (auto spodder : spooders) {
		GameMessages::SendFireEventClientSide(spodder->GetObjectID(), user->GetSystemAddress(), u"toggle", LWOOBJID_EMPTY, 0, 0, user->GetObjectID());
	}

	//Set the flag & mission status:
	auto character = user->GetCharacter();

	if (!character) return;

	character->SetPlayerFlag(ePlayerFlag::CAGED_SPIDER, true);

	//Remove the maelstrom cube:
	auto inv = user->GetComponent<InventoryComponent>();

	if (inv) {
		inv->RemoveItem(14553, 1);
	}
}
