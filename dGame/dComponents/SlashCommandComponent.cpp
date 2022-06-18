#include "SlashCommandComponent.h"

SlashCommandComponent::SlashCommandComponent(Entity* parent): Component(parent) {
	this->RegisterGM(ParseChatMessage::GetId(), (Handler)&SlashCommandComponent::HandleParseChatMessage);
}

SlashCommandComponent::~SlashCommandComponent() {

}
void SlashCommandComponent::HandleParseChatMessage(ParseChatMessage* msg) {
	if (msg->wsString[0] == L'/') {
		SlashCommandHandler::HandleChatCommand(msg->wsString, msg->associate, msg->sysAddr);
	}
}