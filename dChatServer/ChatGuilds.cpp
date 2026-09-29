#include "ChatGuilds.h"

#include <algorithm>
#include <ctime>
#include <memory>

#include "ChatPacketHandler.h"
#include "ChatPackets.h"
#include "Database.h"
#include "dChatFilter.h"
#include "dConfig.h"
#include "dServer.h"
#include "eGameMasterLevel.h"
#include "Game.h"
#include "GeneralUtils.h"
#include "GuildManager.h"
#include "PlayerContainer.h"

namespace {
	std::unique_ptr<GuildManager> g_Guilds;

	std::string Lower(std::string text) {
		std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return text;
	}

	std::optional<GuildManager::OnlinePlayer> Online(const PlayerData& player) {
		if (!player) return std::nullopt;
		return GuildManager::OnlinePlayer{ player.playerID, player.playerName, player.zoneID };
	}
}

void ChatGuilds::Initialize() {
	GuildManager::Settings settings;
	settings.maxMembers = GeneralUtils::TryParse<uint32_t>(Game::config->GetValue("guild_max_members")).value_or(settings.maxMembers);
	settings.inviteTimeout = GeneralUtils::TryParse<int64_t>(Game::config->GetValue("guild_invite_timeout")).value_or(settings.inviteTimeout);

	GuildManager::Hooks hooks;
	hooks.findOnline = [](const LWOOBJID id) { return Online(Game::playerContainer.GetPlayerData(id)); };
	hooks.findOnlineByName = [](const std::string& name) -> std::optional<GuildManager::OnlinePlayer> {
		const auto wanted = Lower(name);
		for (const auto& [id, player] : Game::playerContainer.GetAllPlayers()) {
			if (player && Lower(player.playerName) == wanted) return Online(player);
		}
		return std::nullopt;
	};
	hooks.characterExists = [](const std::string& name) { return Database::Get()->GetCharacterInfo(name).has_value(); };
	hooks.sendToPlayer = [](const LWOOBJID id, const LUBitStream& msg) {
		const auto& player = Game::playerContainer.GetPlayerData(id);
		if (player) ChatPacketHandler::SendRouted(id, player.worldServerSysAddr, msg);
	};
	hooks.sendToWorldOf = [](const LWOOBJID id, const LUBitStream& msg) {
		const auto& player = Game::playerContainer.GetPlayerData(id);
		if (!player) return;
		RakNet::BitStream bitStream;
		msg.WritePacket(bitStream);
		Game::server->Send(bitStream, player.worldServerSysAddr, false);
	};
	hooks.notify = [](const LWOOBJID id, const std::string& text) {
		const auto& player = Game::playerContainer.GetPlayerData(id);
		if (!player) return;
		ChatPackets::Client::GeneralChatMessage chatMessage;
		chatMessage.chatChannel = 4;
		chatMessage.senderName = LUWString("", 33);
		chatMessage.message = GeneralUtils::UTF8ToUTF16(text);
		ChatPacketHandler::SendRouted(id, player.worldServerSysAddr, chatMessage);
	};
	hooks.checkName = [](const std::string& name) {
		if (!Game::chatFilter) return GuildManager::NameCheck::PENDING;
		// The deny list refuses a name; one the allow list doesn't cover waits for a moderator (as pet names do)
		if (Game::chatFilter->HasDenyList() && !Game::chatFilter->IsSentenceOkay(name, eGameMasterLevel::CIVILIAN, false).empty()) return GuildManager::NameCheck::DENIED;
		if (!Game::chatFilter->IsSentenceOkay(name, eGameMasterLevel::CIVILIAN).empty()) return GuildManager::NameCheck::PENDING;
		return GuildManager::NameCheck::APPROVED;
	};
	hooks.now = [] { return static_cast<int64_t>(std::time(nullptr)); };

	g_Guilds = std::make_unique<GuildManager>(*Database::Get(), std::move(hooks), settings);
}

GuildManager& ChatGuilds::Get() {
	return *g_Guilds;
}
