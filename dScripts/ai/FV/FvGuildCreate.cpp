#include "FvGuildCreate.h"

#include "CharacterComponent.h"
#include "ChatPackets.h"
#include "eChatChannel.h"
#include "Entity.h"
#include "PlayerMessages.h"

void FvGuildCreate::OnUse(Entity* self, Entity* user) {
	// A player already in a guild is told so instead of getting the create box, by the Guild Master saying it in local
	// chat to that player: for a local chat line the client puts a chat bubble over the sender, plays its "talk"
	// animation and adds the line under the sender's name (PacketHandler_MSG_CHAT_GENERAL_CHAT_MESSAGE). The text is
	// the client's own MSG_GUILD_ALREADY_IN_GUILD, which the client itself shows only once the box is submitted.
	const auto* characterComponent = user->GetComponent<CharacterComponent>();
	if (characterComponent && characterComponent->GetGuildID() != LWOOBJID_EMPTY) {
		ChatPackets::Client::GeneralChatMessage chat;
		chat.chatChannel = static_cast<uint8_t>(eChatChannel::LOCAL);
		chat.senderName = LUWString("%[Objects_" + std::to_string(self->GetLOT()) + "_name]");
		chat.senderID = self->GetObjectID();
		chat.message = u"You are already in a guild!";
		chat.Send(user->GetSystemAddress());
		return;
	}

	GameMessages::DisplayGuildCreateBox box;
	box.target = user->GetObjectID();
	box.bShow = true;
	box.Send(user->GetSystemAddress());
}
