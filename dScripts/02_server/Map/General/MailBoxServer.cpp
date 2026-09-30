#include "MailBoxServer.h"
#include "Amf3.h"
#include "GameMessages.h"
#include "EffectsMessages.h"
#include "ObjectMessages.h"
#include "Entity.h"

void MailBoxServer::OnUse(Entity* self, Entity* user) {
	AMFArrayValue args;

	args.Insert("state", "Mail");

	GameMessages::UIMessageServerToSingleClient uiMessage;
	uiMessage.target = user->GetObjectID();
	uiMessage.strMessageName = "pushGameState";
	uiMessage.args = std::move(args);
	uiMessage.SendToClient(user->GetSystemAddress());

	// The mailbox's client script marks itself in use (hides the interact icon, refuses another use) until CloseMail
	GameMessages::NotifyClientObject(self->GetObjectID(), u"OpenMail").Send(user->GetSystemAddress());
}

void MailBoxServer::OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2, int32_t param3) {
	if (args == "toggleMail") {
		AMFArrayValue args;
		args.Insert("visible", false);
		GameMessages::UIMessageServerToSingleClient uiMessage;
		uiMessage.target = sender->GetObjectID();
		uiMessage.strMessageName = "ToggleMail";
		uiMessage.args = std::move(args);
		uiMessage.SendToClient(sender->GetSystemAddress());

		GameMessages::NotifyClientObject(self->GetObjectID(), u"CloseMail").Send(sender->GetSystemAddress());
	}
}
