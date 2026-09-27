#include "PropertyBankInteract.h"
#include "EntityManager.h"
#include "GameMessages.h"
#include "EffectsMessages.h"
#include "Amf3.h"
#include "Entity.h"

void PropertyBankInteract::OnStartup(Entity* self) {
	auto* zoneControl = Game::entityManager->GetZoneControlEntity();
	if (zoneControl != nullptr) {
		zoneControl->OnFireEventServerSide(self, "CheckForPropertyOwner");
	}
}

void PropertyBankInteract::OnPlayerLoaded(Entity* self, Entity* player) {
	auto* zoneControl = Game::entityManager->GetZoneControlEntity();
	if (zoneControl != nullptr) {
		zoneControl->OnFireEventServerSide(self, "CheckForPropertyOwner");
	}
}

void PropertyBankInteract::OnUse(Entity* self, Entity* user) {

	AMFArrayValue args;

	args.Insert("state", "bank");

	GameMessages::UIMessageServerToSingleClient uiMessage;
	uiMessage.target = user->GetObjectID();
	uiMessage.strMessageName = "pushGameState";
	uiMessage.args = std::move(args);
	uiMessage.SendToClient(user->GetSystemAddress());

	GameMessages::SendNotifyClientObject(self->GetObjectID(), u"OpenBank", 0, 0, LWOOBJID_EMPTY,
		"", user->GetSystemAddress());
}

void PropertyBankInteract::OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1,
	int32_t param2, int32_t param3) {
	if (args == "ToggleBank") {
		AMFArrayValue amfArgs;

		amfArgs.Insert("visible", false);

		GameMessages::UIMessageServerToSingleClient uiMessage;
		uiMessage.target = sender->GetObjectID();
		uiMessage.strMessageName = "ToggleBank";
		uiMessage.args = std::move(amfArgs);
		uiMessage.SendToClient(sender->GetSystemAddress());

		GameMessages::SendNotifyClientObject(self->GetObjectID(), u"CloseBank", 0, 0, LWOOBJID_EMPTY,
			"", sender->GetSystemAddress());
	}
}
