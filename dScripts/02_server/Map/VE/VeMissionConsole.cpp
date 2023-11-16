#include "VeMissionConsole.h"
#include "InventoryComponent.h"
#include "Character.h"
#include "GameMessages.h"
#include "Loot.h"
#include "eTerminateType.h"

void VeMissionConsole::OnUse(Entity* self, Entity* user) {
	Loot::DropActivityLoot(user, self, 12551);

	auto* inventoryComponent = user->GetComponent<InventoryComponent>();
	if (inventoryComponent != nullptr) {
		inventoryComponent->AddItem(12547, 1, eLootSourceType::ACTIVITY);  // Add the panel required for pickup
	}

	// The flag to set is 101<number>
	const auto flagNumber = self->GetVar<std::u16string>(m_NumberVariable);
	const int32_t flag = std::stoi("101" + GeneralUtils::UTF16ToWTF8(flagNumber));

	auto* character = user->GetCharacter();
	if (character != nullptr) {
		character->SetPlayerFlag(flag, true);
	}

	GameMessages::SendNotifyClientObject(self->GetObjectID(), u"");
	GameMessages::SendTerminateInteraction(user->GetObjectID(), eTerminateType::FROM_INTERACTION, self->GetObjectID());
}
