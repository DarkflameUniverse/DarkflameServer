#include "TokenConsoleServer.h"
#include "InventoryComponent.h"
#include "GameMessages.h"
#include "Character.h"
#include "eReplicaComponentType.h"
#include "eTerminateType.h"
#include "ePlayerFlag.h"

//2021-05-03 - max - added script, omitted some parts related to inheritance in lua which we don't need

void TokenConsoleServer::OnUse(Entity* self, Entity* user) {
	auto* inv = static_cast<InventoryComponent*>(user->GetComponent(eReplicaComponentType::INVENTORY));

	//make sure the user has the required amount of infected bricks
	if (inv && inv->GetLotCount(6194) >= bricksToTake) {
		//yeet the bricks
		inv->RemoveItem(6194, bricksToTake);

		//play sound
		if (self->HasVar(u"sound1")) {
			GameMessages::SendPlayNDAudioEmitter(self, user->GetSystemAddress(), self->GetVarAsString(u"sound1"));
		}

		//figure out which faction the player belongs to:
		auto character = user->GetCharacter();
		if (!character) return;
		// At this point the player has to be in a faction.
		LOT tokenLOT = 0;
		if (character->GetPlayerFlag(ePlayerFlag::VENTURE_FACTION)) //venture
			tokenLOT = 8321;
		else if (character->GetPlayerFlag(ePlayerFlag::ASSEMBLY_FACTION)) //assembly
			tokenLOT = 8318;
		else if (character->GetPlayerFlag(ePlayerFlag::PARADOX_FACTION)) //paradox
			tokenLOT = 8320;
		else if (character->GetPlayerFlag(ePlayerFlag::SENTINEL_FACTION)) //sentinel
			tokenLOT = 8319;
		inv->AddItem(tokenLOT, tokensToGive, eLootSourceType::NONE);
	}

	GameMessages::SendTerminateInteraction(user->GetObjectID(), eTerminateType::FROM_INTERACTION, self->GetObjectID());
}
