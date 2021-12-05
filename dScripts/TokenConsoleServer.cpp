#include "TokenConsoleServer.h"
#include "InventoryComponent.h"
#include "Item.h"
#include "GameMessages.h"
#include "Character.h"
#include "dCommonVars.h"

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

		if (character->GetPlayerFlag(ePlayerFlags::VENTURE_FACTION)) //venture
			inv->AddItem(8321, tokensToGive);
		else if (character->GetPlayerFlag(ePlayerFlags::ASSEMBLY_FACTION)) //assembly
			inv->AddItem(8318, tokensToGive);
		else if (character->GetPlayerFlag(ePlayerFlags::PARADOX_FACTION)) //paradox
			inv->AddItem(8320, tokensToGive);
		else if (character->GetPlayerFlag(ePlayerFlags::SENTINEL_FACTION)) //sentinel
			inv->AddItem(8319, tokensToGive);
	}

	GameMessages::SendTerminateInteraction(user->GetObjectID(), eTerminateType::FROM_INTERACTION, self->GetObjectID());
}