#include "AmConsoleTeleportServer.h"
#include "ChooseYourDestinationNsToNt.h"
#include "AMFFormat.h"

void AmConsoleTeleportServer::OnStartup(Entity* self) {
	self->SetVar(u"teleportAnim", m_TeleportAnim);
	self->SetVar(u"teleportString", m_TeleportString);
	self->SetVar(u"teleportEffectID", m_TeleportEffectID);
	self->SetVar(u"teleportEffectTypes", m_TeleportEffectTypes);
}

void AmConsoleTeleportServer::OnUse(Entity* self, Entity* user) {
	BaseOnUse(self, user);
}

void AmConsoleTeleportServer::OnMessageBoxResponse(Entity* self, Entity* sender, int32_t button, const std::u16string& identifier, const std::u16string& userData) {
	BaseOnMessageBoxResponse(self, sender, button, identifier, userData);
}

void AmConsoleTeleportServer::OnChoiceBoxResponse(Entity* self, Entity* sender, int32_t button, const std::u16string& buttonIdentifier, const std::u16string& identifier) {

}

void AmConsoleTeleportServer::OnTimerDone(Entity* self, std::string timerName) {
	BaseOnTimerDone(self, timerName);
}

void AmConsoleTeleportServer::OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2, int32_t param3) {
	BaseOnFireEventServerSide(self, sender, args, param1, param2, param3);
}
