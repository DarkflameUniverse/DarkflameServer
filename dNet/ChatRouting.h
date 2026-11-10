#ifndef __CHATROUTING__H__
#define __CHATROUTING__H__

#include <cstdint>
#include <optional>

#include "MessageType/Chat.h"
#include "eGameMasterLevel.h"

namespace ChatRouting {
	// A routed packet is the world header, the size of the chat packet and then the chat packet itself.
	// The chat packet's message ID sits at this offset and its contents start after its own header.
	constexpr uint32_t MessageIdOffset = 14;
	constexpr uint32_t ContentOffset = 23;

	/**
	 * The chat messages a client sends through a world server, with the GM level each one needs.
	 * Every other chat message only ever comes from a server, so it has no level and is not routed.
	 */
	constexpr std::optional<eGameMasterLevel> GetRequiredGMLevel(const MessageType::Chat messageId) {
		switch (messageId) {
		case MessageType::Chat::GENERAL_CHAT_MESSAGE:
		case MessageType::Chat::PRIVATE_CHAT_MESSAGE:
		case MessageType::Chat::USER_CHANNEL_CHAT_MESSAGE:
		case MessageType::Chat::ADD_FRIEND_REQUEST:
		case MessageType::Chat::ADD_FRIEND_RESPONSE:
		case MessageType::Chat::REMOVE_FRIEND:
		case MessageType::Chat::GET_FRIENDS_LIST:
		case MessageType::Chat::ADD_IGNORE:
		case MessageType::Chat::REMOVE_IGNORE:
		case MessageType::Chat::GET_IGNORE_LIST:
		case MessageType::Chat::TEAM_MISSED_INVITE_CHECK:
		case MessageType::Chat::TEAM_INVITE:
		case MessageType::Chat::TEAM_INVITE_RESPONSE:
		case MessageType::Chat::TEAM_KICK:
		case MessageType::Chat::TEAM_LEAVE:
		case MessageType::Chat::TEAM_SET_LOOT:
		case MessageType::Chat::TEAM_SET_LEADER:
		case MessageType::Chat::TEAM_GET_STATUS:
		case MessageType::Chat::GUILD_CREATE:
		case MessageType::Chat::GUILD_INVITE:
		case MessageType::Chat::GUILD_INVITE_RESPONSE:
		case MessageType::Chat::GUILD_LEAVE:
		case MessageType::Chat::GUILD_KICK:
		case MessageType::Chat::GUILD_GET_STATUS:
		case MessageType::Chat::GUILD_GET_ALL:
		case MessageType::Chat::MAIL:
		case MessageType::Chat::REQUEST_MINIMUM_CHAT_MODE:
		case MessageType::Chat::REQUEST_MINIMUM_CHAT_MODE_PRIVATE:
		case MessageType::Chat::WHO:
			return eGameMasterLevel::CIVILIAN;
		// The same level the showall chat command needs
		case MessageType::Chat::SHOW_ALL:
		case MessageType::Chat::GM_CLOSE_PRIVATE_CHAT_WINDOW:
			return eGameMasterLevel::JUNIOR_MODERATOR;
		default:
			return std::nullopt;
		}
	}

	constexpr bool CanRoute(const MessageType::Chat messageId, const eGameMasterLevel senderLevel) {
		const auto requiredLevel = GetRequiredGMLevel(messageId);
		return requiredLevel && senderLevel >= *requiredLevel;
	}
}

#endif  //!__CHATROUTING__H__
