#include "GuildCommands.h"

#include <cctype>

#include "Character.h"
#include "ChatPackets.h"
#include "ChatServerLink.h"
#include "dChatFilter.h"
#include "Entity.h"
#include "eChatChannel.h"
#include "eGameMasterLevel.h"
#include "eGuildRank.h"
#include "Game.h"
#include "GeneralUtils.h"
#include "PlayerMessages.h"
#include "User.h"

namespace {
	std::string Lower(std::string text) {
		for (auto& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		return text;
	}

	std::string Trimmed(const std::string& text) {
		const auto start = text.find_first_not_of(' ');
		if (start == std::string::npos) return "";
		return text.substr(start, text.find_last_not_of(' ') - start + 1);
	}

	LUWString PlayerName(const std::string& name) {
		return LUWString(GeneralUtils::UTF8ToUTF16(name), 33);
	}

	void SendRank(Entity* entity, const std::string& name, const eGuildRank rank) {
		ChatPackets::GuildSetRank request;
		request.playerID = entity->GetObjectID();
		request.targetPlayer = PlayerName(name);
		request.rank = static_cast<uint8_t>(rank);
		ChatServerLink::Send(request);
	}
}

void GuildCommands::Chat(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
	const auto message = Trimmed(args);
	if (message.empty()) return;
	auto* character = entity->GetCharacter();
	if (!character) return;
	auto* user = character->GetParentUser();
	if (user && user->GetIsMuted()) {
		character->SendMuteNotice();
		return;
	}
	// The same filter as the zone's chat
	if (!Game::chatFilter->IsSentenceOkay(message, character->GetGMLevel()).empty()) {
		ChatPackets::SendSystemMessage(sysAddr, u"Your message was not sent to your guild.");
		return;
	}

	auto text = GeneralUtils::UTF8ToUTF16(message);
	ChatPackets::GeneralChatMessage chat;
	chat.playerID = entity->GetObjectID();
	chat.chatChannel = eChatChannel::GUILD;
	chat.messageLength = static_cast<uint32_t>(text.size());
	chat.senderName = PlayerName(character->GetName());
	chat.senderID = entity->GetObjectID();
	chat.senderGMLevel = static_cast<uint8_t>(character->GetGMLevel());
	chat.message = LUWString(text, static_cast<uint32_t>(text.size()));
	ChatServerLink::Send(chat);
}

void GuildCommands::OpenCreateBox(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
	GameMessages::DisplayGuildCreateBox box;
	box.target = entity->GetObjectID();
	box.Send(sysAddr);
}

void GuildCommands::Kick(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
	const auto name = Trimmed(args);
	if (name.empty()) {
		ChatPackets::SendSystemMessage(sysAddr, u"Usage: /gkick <name>");
		return;
	}
	ChatPackets::GuildKick request;
	request.playerID = entity->GetObjectID();
	request.kickedPlayer = PlayerName(name);
	ChatServerLink::Send(request);
}

void GuildCommands::Rank(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
	const auto split = GeneralUtils::SplitString(Trimmed(args), ' ');
	const std::string rankName = split.size() == 2 ? Lower(split[1]) : "";
	eGuildRank rank = eGuildRank::NONE;
	if (rankName == "officer") rank = eGuildRank::OFFICER;
	else if (rankName == "veteran") rank = eGuildRank::VETERAN;
	else if (rankName == "recruit") rank = eGuildRank::RECRUIT;
	if (rank == eGuildRank::NONE) {
		ChatPackets::SendSystemMessage(sysAddr, u"Usage: /grank <name> <officer|veteran|recruit>");
		return;
	}
	SendRank(entity, split[0], rank);
}

void GuildCommands::Leader(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
	const auto name = Trimmed(args);
	if (name.empty()) {
		ChatPackets::SendSystemMessage(sysAddr, u"Usage: /gleader <name>");
		return;
	}
	SendRank(entity, name, eGuildRank::LEADER);
}

void GuildCommands::Disband(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
	if (Trimmed(args) != "confirm") {
		ChatPackets::SendSystemMessage(sysAddr, u"This removes every member and the guild itself. To do it, type /gdisband confirm");
		return;
	}
	ChatPackets::GuildDisband request;
	request.playerID = entity->GetObjectID();
	ChatServerLink::Send(request);
}
