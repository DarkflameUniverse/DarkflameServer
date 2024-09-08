#include "NsTokenConsoleServer.h"
#include "InventoryComponent.h"
#include "GameMessages.h"
#include "Character.h"
#include "MissionComponent.h"
#include "QuickBuildComponent.h"
#include "eTerminateType.h"
#include "ePlayerFlag.h"

void NsTokenConsoleServer::OnStartup(Entity* self) {

}

void NsTokenConsoleServer::OnUse(Entity* self, Entity* user) {
	auto* quickBuildComponent = self->GetComponent<QuickBuildComponent>();

	if (quickBuildComponent == nullptr) {
		return;
	}

	if (quickBuildComponent->GetState() != eQuickBuildState::COMPLETED) {
		return;
	}

	auto* inventoryComponent = user->GetComponent<InventoryComponent>();
	auto* missionComponent = user->GetComponent<MissionComponent>();
	auto* character = user->GetCharacter();

	if (inventoryComponent == nullptr || missionComponent == nullptr || character == nullptr) {
		return;
	}

	if (inventoryComponent->GetLotCount(6194) < 25) {
		return;
	}

	inventoryComponent->RemoveItem(6194, 25);

	const auto useSound = self->GetVar<std::string>(u"sound1");

	if (!useSound.empty()) {
		GameMessages::SendPlayNDAudioEmitter(self, user->GetSystemAddress(), useSound);
	}

	// Player must be in faction to interact with this entity.
	LOT tokenLOT = 0;
	if (character->GetPlayerFlag(ePlayerFlag::VENTURE_FACTION)) //venture
		tokenLOT = 8321;
	else if (character->GetPlayerFlag(ePlayerFlag::ASSEMBLY_FACTION)) //assembly
		tokenLOT = 8318;
	else if (character->GetPlayerFlag(ePlayerFlag::PARADOX_FACTION)) //paradox
		tokenLOT = 8320;
	else if (character->GetPlayerFlag(ePlayerFlag::SENTINEL_FACTION)) //sentinel
		tokenLOT = 8319;

	inventoryComponent->AddItem(tokenLOT, 5, eLootSourceType::NONE);

	missionComponent->ForceProgressTaskType(863, 1, 1, false);

	GameMessages::SendTerminateInteraction(user->GetObjectID(), eTerminateType::FROM_INTERACTION, self->GetObjectID());
}
