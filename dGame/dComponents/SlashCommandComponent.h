#pragma once

#include "CDClientManager.h"
#include "Component.h"
#include "Entity.h"
#include "GameMessages.h"
#include "RakNetTypes.h"

#include "SlashCommandHandler.h"

#include "Client/ParseChatMessage.h"

/**
 * A component for handling slash commands for the player
 */
class SlashCommandComponent : public Component {
public:
	static const uint32_t ComponentType = COMPONENT_TYPE_SLASH_COMMAND;

	SlashCommandComponent(Entity* parent);
	~SlashCommandComponent();

	void HandleParseChatMessage(ParseChatMessage* msg);
private:
};

