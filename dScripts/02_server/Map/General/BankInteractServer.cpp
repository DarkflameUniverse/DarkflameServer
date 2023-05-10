#include "BankInteractServer.h"
#include "GameMessages.h"
#include "Entity.h"
#include "Amf3.h"

void BankInteractServer::OnUse(Entity* self, Entity* user) {
	AMFArrayValue args;

	args.Insert("state", "bank");

	GameMessages::SendUIMessageServerToSingleClient(user, user->GetSystemAddress(), "pushGameState", &args);
}

void BankInteractServer::OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1,
	int32_t param2, int32_t param3) {
	if (args == "ToggleBank") {
		AMFArrayValue args;

		args.Insert("visible", false);

		GameMessages::SendUIMessageServerToSingleClient(sender, sender->GetSystemAddress(), "ToggleBank", &args);

		GameMessages::SendNotifyClientObject(self->GetObjectID(), u"CloseBank", 0, 0, LWOOBJID_EMPTY, "", sender->GetSystemAddress());
	}
}
