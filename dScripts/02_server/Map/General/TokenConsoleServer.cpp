#include "TokenConsoleServer.h"

#include "InventoryComponent.h"
#include "GameMessages.h"
#include "eTerminateType.h"
#include "ePlayerFlag.h"

//2021-05-03 - max - added script, omitted some parts related to inheritance in lua which we don't need

void TokenConsoleServer::OnUse(Entity* self, Entity* user) {
	auto* inv = user->GetComponent<InventoryComponent>();

	//make sure the user has the required amount of infected bricks
	if (inv && inv->GetLotCount(6194) >= bricksToTake) {
		//yeet the bricks
		inv->RemoveItem(6194, bricksToTake);

		//play sound
		if (self->HasVar(u"sound1")) {
			GameMessages::SendPlayNDAudioEmitter(self, user->GetSystemAddress(), self->GetVarAsString(u"sound1"));
		}

		//figure out which faction the player belongs to:
		// At this point the player has to be in a faction.

		GameMessages::GetFlag getFlag{};
		getFlag.target = user->GetObjectID();
		LOT tokenLOT = 0;
		if (getFlag.iFlagId = ePlayerFlag::VENTURE_FACTION, SEND_ENTITY_MSG(getFlag) && getFlag.bFlag) //venture
			tokenLOT = 8321;
		else if (getFlag.iFlagId = ePlayerFlag::ASSEMBLY_FACTION, SEND_ENTITY_MSG(getFlag) && getFlag.bFlag) //assembly
			tokenLOT = 8318;
		else if (getFlag.iFlagId = ePlayerFlag::PARADOX_FACTION, SEND_ENTITY_MSG(getFlag) && getFlag.bFlag) //paradox
			tokenLOT = 8320;
		else if (getFlag.iFlagId = ePlayerFlag::SENTINEL_FACTION, SEND_ENTITY_MSG(getFlag) && getFlag.bFlag) //sentinel
			tokenLOT = 8319;
		inv->AddItem(tokenLOT, tokensToGive, eLootSourceType::NONE);
	}

	GameMessages::SendTerminateInteraction(user->GetObjectID(), eTerminateType::FROM_INTERACTION, self->GetObjectID());
}
