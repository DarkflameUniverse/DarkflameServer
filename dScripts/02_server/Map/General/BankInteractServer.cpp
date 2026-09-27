#include "BankInteractServer.h"
#include "GameMessages.h"
#include "EffectsMessages.h"
#include "Entity.h"
#include "Amf3.h"

void BankInteractServer::OnUse(Entity* self, Entity* user) {
	AMFArrayValue args;

	args.Insert("state", "bank");

	GameMessages::UIMessageServerToSingleClient uiMessage;
	uiMessage.target = user->GetObjectID();
	uiMessage.strMessageName = "pushGameState";
	uiMessage.args = std::move(args);
	uiMessage.SendToClient(user->GetSystemAddress());
}

void BankInteractServer::OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1,
	int32_t param2, int32_t param3) {
	if (args == "ToggleBank") {
		AMFArrayValue args;

		args.Insert("visible", false);

		GameMessages::UIMessageServerToSingleClient uiMessage;
		uiMessage.target = sender->GetObjectID();
		uiMessage.strMessageName = "ToggleBank";
		uiMessage.args = std::move(args);
		uiMessage.SendToClient(sender->GetSystemAddress());

		GameMessages::SendNotifyClientObject(self->GetObjectID(), u"CloseBank", 0, 0, LWOOBJID_EMPTY, "", sender->GetSystemAddress());
	}
}
