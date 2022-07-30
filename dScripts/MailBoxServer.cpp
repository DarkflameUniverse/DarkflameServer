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
