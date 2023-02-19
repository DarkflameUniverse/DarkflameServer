#include "NsLupTeleport.h"
#include "dZoneManager.h"
#include "GameMessages.h"
#include "Amf3.h"

void NsLupTeleport::OnStartup(Entity* self) {
	self->SetVar(u"currentZone", (int32_t)dZoneManager::Instance()->GetZoneID().GetMapID());
	self->SetVar(u"choiceZone", m_ChoiceZoneID);
	self->SetVar(u"teleportAnim", m_TeleportAnim);
	self->SetVar(u"teleportString", m_TeleportString);
	self->SetVar(u"spawnPoint", m_SpawnPoint);

	args = {};

	args.InsertAssociative("callbackClient", std::to_string(self->GetObjectID()));
	args.InsertAssociative("strIdentifier", "choiceDoor");
	args.InsertAssociative("title", "%[UI_CHOICE_DESTINATION]");

	AMFArrayValue* choiceOptions = args.InsertAssociativeArray("options");

	{
		AMFArrayValue* nsArgs = choiceOptions->PushDenseArray();

		nsArgs->InsertAssociative("image", "textures/ui/zone_thumnails/Nimbus_Station.dds");
		nsArgs->InsertAssociative("caption", "%[UI_CHOICE_NS]");
		nsArgs->InsertAssociative("identifier", "zoneID_1200");
		nsArgs->InsertAssociative("tooltipText", "%[UI_CHOICE_NS_HOVER]");
	}

	{
		AMFArrayValue* ntArgs = choiceOptions->PushDenseArray();

		ntArgs->InsertAssociative("image", "textures/ui/zone_thumnails/Nexus_Tower.dds");
		ntArgs->InsertAssociative("caption", "%[UI_CHOICE_NT]");
		ntArgs->InsertAssociative("identifier", "zoneID_1900");
		ntArgs->InsertAssociative("tooltipText", "%[UI_CHOICE_NT_HOVER]");
	}
}

void NsLupTeleport::OnUse(Entity* self, Entity* user) {
	auto* player = user;

	if (CheckChoice(self, player)) {
		GameMessages::SendUIMessageServerToSingleClient(player, player->GetSystemAddress(), "QueueChoiceBox", &args);
	} else {
		BaseOnUse(self, player);
	}
}

void NsLupTeleport::OnMessageBoxResponse(Entity* self, Entity* sender, int32_t button, const std::u16string& identifier, const std::u16string& userData) {
	BaseOnMessageBoxResponse(self, sender, button, identifier, userData);
}

void NsLupTeleport::OnChoiceBoxResponse(Entity* self, Entity* sender, int32_t button, const std::u16string& buttonIdentifier, const std::u16string& identifier) {
	BaseChoiceBoxRespond(self, sender, button, buttonIdentifier, identifier);
}

void NsLupTeleport::OnTimerDone(Entity* self, std::string timerName) {
	BaseOnTimerDone(self, timerName);
}

void NsLupTeleport::OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2, int32_t param3) {
	BaseOnFireEventServerSide(self, sender, args, param1, param2, param3);
}
