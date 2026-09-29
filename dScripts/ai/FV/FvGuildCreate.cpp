#include "FvGuildCreate.h"

#include "CharacterComponent.h"
#include "ChatPackets.h"
#include "EffectsMessages.h"
#include "Entity.h"
#include "PlayerMessages.h"

void FvGuildCreate::OnUse(Entity* self, Entity* user) {
	// A player already in a guild is told so instead of getting the create box: the Guild Master says it in a chat
	// bubble over its head and in system chat. The client itself says the same (MSG_GUILD_ALREADY_IN_GUILD, system chat)
	// only once the box is submitted (LWOGuildComponent::SendTMPGuildCreate).
	const auto* characterComponent = user->GetComponent<CharacterComponent>();
	if (characterComponent && characterComponent->GetGuildID() != LWOOBJID_EMPTY) {
		constexpr auto text = u"You are already in a guild!";
		GameMessages::DisplayChatBubble bubble;
		bubble.target = self->GetObjectID();
		bubble.wsText = text;
		bubble.Send(user->GetSystemAddress());
		ChatPackets::SendSystemMessage(user->GetSystemAddress(), text);
		return;
	}

	GameMessages::DisplayGuildCreateBox box;
	box.target = user->GetObjectID();
	box.bShow = true;
	box.Send(user->GetSystemAddress());
}
