#include "FvGuildCreate.h"

#include "CharacterComponent.h"
#include "ChatPackets.h"
#include "Entity.h"
#include "PlayerMessages.h"

void FvGuildCreate::OnUse(Entity* self, Entity* user) {
	// A player already in a guild is told so instead of getting the create box. The client itself says the same
	// (MSG_GUILD_ALREADY_IN_GUILD, system chat) only once the box is submitted (LWOGuildComponent::SendTMPGuildCreate).
	const auto* characterComponent = user->GetComponent<CharacterComponent>();
	if (characterComponent && characterComponent->GetGuildID() != LWOOBJID_EMPTY) {
		ChatPackets::SendSystemMessage(user->GetSystemAddress(), u"You are already in a guild!");
		return;
	}

	GameMessages::DisplayGuildCreateBox box;
	box.target = user->GetObjectID();
	box.bShow = true;
	box.Send(user->GetSystemAddress());
}
