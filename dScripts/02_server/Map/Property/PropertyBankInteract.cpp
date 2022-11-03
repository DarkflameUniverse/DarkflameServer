#include "PropertyBankInteract.h"
#include "EntityManager.h"
#include "GameMessages.h"

void PropertyBankInteract::OnStartup(Entity* self) {
	auto* zoneControl = EntityManager::Instance()->GetZoneControlEntity();
	if (zoneControl != nullptr) {
		zoneControl->OnFireEventServerSide(self, "CheckForPropertyOwner");
	}
}

void PropertyBankInteract::OnPlayerLoaded(Entity* self, Entity* player) {
	auto* zoneControl = EntityManager::Instance()->GetZoneControlEntity();
	if (zoneControl != nullptr) {
		zoneControl->OnFireEventServerSide(self, "CheckForPropertyOwner");
	}
}

void PropertyBankInteract::OnUse(Entity* self, Entity* user) {

	AMFArrayValue args;
	auto* value = new AMFStringValue();
	value->SetStringValue("bank");
	args.InsertValue("state", value);

	GameMessages::SendUIMessageServerToSingleClient(user, user->GetSystemAddress(), "pushGameState", &args);

	GameMessages::SendNotifyClientObject(self->GetObjectID(), u"OpenBank", 0, 0, LWOOBJID_EMPTY,
		"", user->GetSystemAddress());
}

void PropertyBankInteract::OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1,
	int32_t param2, int32_t param3) {
	if (args == "ToggleBank") {
		AMFArrayValue amfArgs;
		amfArgs.InsertValue("visible", new AMFFalseValue());

		GameMessages::SendUIMessageServerToSingleClient(sender, sender->GetSystemAddress(), "ToggleBank", &amfArgs);

		GameMessages::SendNotifyClientObject(self->GetObjectID(), u"CloseBank", 0, 0, LWOOBJID_EMPTY,
			"", sender->GetSystemAddress());
	}
}
