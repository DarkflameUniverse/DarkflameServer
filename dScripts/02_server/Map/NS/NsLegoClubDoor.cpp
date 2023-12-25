#include "NsLegoClubDoor.h"
#include "dZoneManager.h"
#include "GameMessages.h"
#include "Amf3.h"

void NsLegoClubDoor::OnStartup(Entity* self) {
	self->SetVar(u"currentZone", static_cast<int32_t>Game::zoneManager->GetZoneID().GetMapID());
	self->SetVar(u"choiceZone", m_ChoiceZoneID);
	self->SetVar(u"teleportAnim", m_TeleportAnim);
	self->SetVar(u"teleportString", m_TeleportString);
	self->SetVar(u"spawnPoint", m_SpawnPoint);

	args = {};

	args.Insert("callbackClient", std::to_string(self->GetObjectID()));
	args.Insert("strIdentifier", "choiceDoor");
	args.Insert("title", "%[UI_CHOICE_DESTINATION]");

	AMFArrayValue* choiceOptions = args.InsertArray("options");

	{
		AMFArrayValue* nsArgs = choiceOptions->PushArray();

		nsArgs->Insert("image", "textures/ui/zone_thumnails/Nimbus_Station.dds");
		nsArgs->Insert("caption", "%[UI_CHOICE_NS]");
		nsArgs->Insert("identifier", "zoneID_1200");
		nsArgs->Insert("tooltipText", "%[UI_CHOICE_NS_HOVER]");
	}

	{
		AMFArrayValue* ntArgs = choiceOptions->PushArray();

		ntArgs->Insert("image", "textures/ui/zone_thumnails/Nexus_Tower.dds");
		ntArgs->Insert("caption", "%[UI_CHOICE_NT]");
		ntArgs->Insert("identifier", "zoneID_1900");
		ntArgs->Insert("tooltipText", "%[UI_CHOICE_NT_HOVER]");
	}

	options = choiceOptions;
}

void NsLegoClubDoor::OnUse(Entity* self, Entity* user) {
	auto* player = user;

	if (CheckChoice(self, player)) {
		AMFArrayValue multiArgs;

		multiArgs.Insert("callbackClient", std::to_string(self->GetObjectID()));
		multiArgs.Insert("strIdentifier", "choiceDoor");
		multiArgs.Insert("title", "%[UI_CHOICE_DESTINATION]");
		multiArgs.Insert("options", static_cast<AMFBaseValue*>(options));

		GameMessages::SendUIMessageServerToSingleClient(player, player->GetSystemAddress(), "QueueChoiceBox", multiArgs);

		multiArgs.Remove("options", false); // We do not want the local amf to delete the options!
	} else if (self->GetVar<int32_t>(u"currentZone") != m_ChoiceZoneID) {
		AMFArrayValue multiArgs;
		multiArgs.Insert("state", "Lobby");

		AMFArrayValue* context = multiArgs.InsertArray("context");
		context->Insert("user", std::to_string(player->GetObjectID()));
		context->Insert("callbackObj", std::to_string(self->GetObjectID()));
		context->Insert("HelpVisible", "show");
		context->Insert("type", "Lego_Club_Valid");

		GameMessages::SendUIMessageServerToSingleClient(player, player->GetSystemAddress(), "pushGameState", multiArgs);
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
