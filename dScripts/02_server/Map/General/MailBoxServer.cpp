#include "MailBoxServer.h"
#include "AMFFormat.h"
#include "GameMessages.h"

void MailBoxServer::OnUse(Entity* self, Entity* user) {
	AMFStringValue* value = new AMFStringValue();
	value->SetStringValue("Mail");
	AMFArrayValue args;
	args.InsertValue("state", value);
	GameMessages::SendUIMessageServerToSingleClient(user, user->GetSystemAddress(), "pushGameState", &args);
}

void MailBoxServer::OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2, int32_t param3) {
	if (args == "toggleMail") {
		AMFArrayValue args;
		args.InsertValue("visible", new AMFFalseValue());
		GameMessages::SendUIMessageServerToSingleClient(sender, sender->GetSystemAddress(), "ToggleMail", &args);
	}
}
