#include "TokenConsoleServer.h"
#include "InventoryComponent.h"
#include "GameMessages.h"
#include "Character.h"

//2021-05-03 - max - added script, omitted some parts related to inheritance in lua which we don't need

void TokenConsoleServer::OnUse(Entity* self, Entity* user) {
	auto* inv = static_cast<InventoryComponent*>(user->GetComponent(COMPONENT_TYPE_INVENTORY));

	//make sure the user has the required amount of infected bricks
	if (inv && inv->GetLotCount(6194) >= bricksToTake) {
		//yeet the bricks
		inv->RemoveItem(6194, bricksToTake);

		//play sound
		GameMessages::SendPlayNDAudioEmitter(self, user->GetSystemAddress(), "947d0d52-c7f8-4516-8dee-e1593a7fd1d1");

		//figure out which faction the player belongs to:
		auto character = user->GetCharacter();
		if (!character) return;
		// At this point the player has to be in a faction.
		LOT tokenLOT = 0;
		if (character->GetPlayerFlag(ePlayerFlags::VENTURE_FACTION)) //venture
			tokenLOT = 8321;
		else if (character->GetPlayerFlag(ePlayerFlags::ASSEMBLY_FACTION)) //assembly
			tokenLOT = 8318;
		else if (character->GetPlayerFlag(ePlayerFlags::PARADOX_FACTION)) //paradox
			tokenLOT = 8320;
		else if (character->GetPlayerFlag(ePlayerFlags::SENTINEL_FACTION)) //sentinel
			tokenLOT = 8319;
		inv->AddItem(tokenLOT, tokensToGive, eLootSourceType::LOOT_SOURCE_NONE);
	}

	GameMessages::SendTerminateInteraction(user->GetObjectID(), eTerminateType::FROM_INTERACTION, self->GetObjectID());
}
