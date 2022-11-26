#include "NsLupTeleport.h"
#include "dZoneManager.h"
#include "GameMessages.h"
#include "AMFFormat.h"

void NsLupTeleport::OnStartup(Entity* self) {
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

	args.InsertValue("options", choiceOptions);
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
