#include "MailBoxServer.h"
#include "Amf3.h"
#include "GameMessages.h"
#include "Entity.h"

void MailBoxServer::OnUse(Entity* self, Entity* user) {
	AMFArrayValue args;

	args.Insert("state", "Mail");

	GameMessages::SendUIMessageServerToSingleClient(user, user->GetSystemAddress(), "pushGameState", &args);
}

void MailBoxServer::OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2, int32_t param3) {
	if (args == "toggleMail") {
		AMFArrayValue args;
		args.Insert("visible", false);
		GameMessages::SendUIMessageServerToSingleClient(sender, sender->GetSystemAddress(), "ToggleMail", &args);
	}
}
