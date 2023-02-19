#include "NsLegoClubDoor.h"
#include "dZoneManager.h"
#include "GameMessages.h"
#include "Amf3.h"

void NsLegoClubDoor::OnStartup(Entity* self) {
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

	options = choiceOptions;
}

void NsLegoClubDoor::OnUse(Entity* self, Entity* user) {
	auto* player = user;

	if (CheckChoice(self, player)) {
		AMFArrayValue multiArgs;

		multiArgs.InsertAssociative("callbackClient", std::to_string(self->GetObjectID()));
		multiArgs.InsertAssociative("strIdentifier", "choiceDoor");
		multiArgs.InsertAssociative("title", "%[UI_CHOICE_DESTINATION]");
		multiArgs.RegisterAssociative("options", options);

		GameMessages::SendUIMessageServerToSingleClient(player, player->GetSystemAddress(), "QueueChoiceBox", &multiArgs);

		multiArgs.RemoveAssociative("options", false); // We do not want the local amf to delete the options!
	} else if (self->GetVar<int32_t>(u"currentZone") != m_ChoiceZoneID) {
		AMFArrayValue multiArgs;
		multiArgs.InsertAssociative("state", "Lobby");

		AMFArrayValue* context = multiArgs.InsertAssociativeArray("context");
		context->InsertAssociative("user", std::to_string(player->GetObjectID()));
		context->InsertAssociative("callbackObj", std::to_string(self->GetObjectID()));
		context->InsertAssociative("HelpVisible", "show");
		context->InsertAssociative("type", "Lego_Club_Valid");

		GameMessages::SendUIMessageServerToSingleClient(player, player->GetSystemAddress(), "pushGameState", &multiArgs);
	} else {
		BaseOnUse(self, player);
	}
}

void NsLegoClubDoor::OnMessageBoxResponse(Entity* self, Entity* sender, int32_t button, const std::u16string& identifier, const std::u16string& userData) {
	std::u16string strIdentifier = identifier;

	if (strIdentifier == u"PlayButton" || strIdentifier == u"CloseButton") {
		strIdentifier = u"TransferBox";
	}

	BaseOnMessageBoxResponse(self, sender, button, strIdentifier, userData);
}

void NsLegoClubDoor::OnChoiceBoxResponse(Entity* self, Entity* sender, int32_t button, const std::u16string& buttonIdentifier, const std::u16string& identifier) {
	BaseChoiceBoxRespond(self, sender, button, buttonIdentifier, identifier);
}

void NsLegoClubDoor::OnTimerDone(Entity* self, std::string timerName) {
	BaseOnTimerDone(self, timerName);
}

void NsLegoClubDoor::OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2, int32_t param3) {
	BaseOnFireEventServerSide(self, sender, args, param1, param2, param3);
}
