#include "NsLegoClubDoor.h"
#include "dZoneManager.h"
#include "GameMessages.h"
#include "AMFFormat.h"

void NsLegoClubDoor::OnStartup(Entity* self) {
	self->SetVar(u"currentZone", (int32_t)dZoneManager::Instance()->GetZoneID().GetMapID());
	self->SetVar(u"choiceZone", m_ChoiceZoneID);
	self->SetVar(u"teleportAnim", m_TeleportAnim);
	self->SetVar(u"teleportString", m_TeleportString);
	self->SetVar(u"spawnPoint", m_SpawnPoint);

	args = {};

	AMFStringValue* callbackClient = new AMFStringValue();
	callbackClient->SetStringValue(std::to_string(self->GetObjectID()));
	args.InsertValue("callbackClient", callbackClient);

	AMFStringValue* strIdentifier = new AMFStringValue();
	strIdentifier->SetStringValue("choiceDoor");
	args.InsertValue("strIdentifier", strIdentifier);

	AMFStringValue* title = new AMFStringValue();
	title->SetStringValue("%[UI_CHOICE_DESTINATION]");
	args.InsertValue("title", title);

	AMFArrayValue* choiceOptions = new AMFArrayValue();

	{
		AMFArrayValue* nsArgs = new AMFArrayValue();

		AMFStringValue* image = new AMFStringValue();
		image->SetStringValue("textures/ui/zone_thumnails/Nimbus_Station.dds");
		nsArgs->InsertValue("image", image);

		AMFStringValue* caption = new AMFStringValue();
		caption->SetStringValue("%[UI_CHOICE_NS]");
		nsArgs->InsertValue("caption", caption);

		AMFStringValue* identifier = new AMFStringValue();
		identifier->SetStringValue("zoneID_1200");
		nsArgs->InsertValue("identifier", identifier);

		AMFStringValue* tooltipText = new AMFStringValue();
		tooltipText->SetStringValue("%[UI_CHOICE_NS_HOVER]");
		nsArgs->InsertValue("tooltipText", tooltipText);

		choiceOptions->PushBackValue(nsArgs);
	}

	{
		AMFArrayValue* ntArgs = new AMFArrayValue();

		AMFStringValue* image = new AMFStringValue();
		image->SetStringValue("textures/ui/zone_thumnails/Nexus_Tower.dds");
		ntArgs->InsertValue("image", image);

		AMFStringValue* caption = new AMFStringValue();
		caption->SetStringValue("%[UI_CHOICE_NT]");
		ntArgs->InsertValue("caption", caption);

		AMFStringValue* identifier = new AMFStringValue();
		identifier->SetStringValue("zoneID_1900");
		ntArgs->InsertValue("identifier", identifier);

		AMFStringValue* tooltipText = new AMFStringValue();
		tooltipText->SetStringValue("%[UI_CHOICE_NT_HOVER]");
		ntArgs->InsertValue("tooltipText", tooltipText);

		choiceOptions->PushBackValue(ntArgs);
	}

	options = choiceOptions;

	args.InsertValue("options", choiceOptions);
}

void NsLegoClubDoor::OnUse(Entity* self, Entity* user) {
	auto* player = user;

	if (CheckChoice(self, player)) {
		AMFArrayValue* multiArgs = new AMFArrayValue();

		AMFStringValue* callbackClient = new AMFStringValue();
		callbackClient->SetStringValue(std::to_string(self->GetObjectID()));
		multiArgs->InsertValue("callbackClient", callbackClient);

		AMFStringValue* strIdentifier = new AMFStringValue();
		strIdentifier->SetStringValue("choiceDoor");
		multiArgs->InsertValue("strIdentifier", strIdentifier);

		AMFStringValue* title = new AMFStringValue();
		title->SetStringValue("%[UI_CHOICE_DESTINATION]");
		multiArgs->InsertValue("title", title);

		multiArgs->InsertValue("options", options);

		GameMessages::SendUIMessageServerToSingleClient(player, player->GetSystemAddress(), "QueueChoiceBox", multiArgs);
	} else if (self->GetVar<int32_t>(u"currentZone") != m_ChoiceZoneID) {
		AMFArrayValue* multiArgs = new AMFArrayValue();

		AMFStringValue* state = new AMFStringValue();
		state->SetStringValue("Lobby");
		multiArgs->InsertValue("state", state);

		AMFArrayValue* context = new AMFArrayValue();

		AMFStringValue* user = new AMFStringValue();
		user->SetStringValue(std::to_string(player->GetObjectID()));
		context->InsertValue("user", user);

		AMFStringValue* callbackObj = new AMFStringValue();
		callbackObj->SetStringValue(std::to_string(self->GetObjectID()));
		context->InsertValue("callbackObj", callbackObj);

		AMFStringValue* helpVisible = new AMFStringValue();
		helpVisible->SetStringValue("show");
		context->InsertValue("HelpVisible", helpVisible);

		AMFStringValue* type = new AMFStringValue();
		type->SetStringValue("Lego_Club_Valid");
		context->InsertValue("type", type);

		multiArgs->InsertValue("context", context);

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
