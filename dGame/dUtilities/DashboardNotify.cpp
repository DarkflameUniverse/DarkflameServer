#include "DashboardNotify.h"

#include <chrono>
#include <set>

#include "BitStreamUtils.h"
#include "DataChanged.h"
#include "Game.h"
#include "dServer.h"
#include "MessageType/Master.h"
#include "ServiceType.h"
#include "DashboardMessages.h"
#include "PlayerManager.h"
#include "Entity.h"
#include "Character.h"
#include "dZoneManager.h"
#include "GameMessages.h"
#include "ChatPackets.h"
#include "Amf3.h"
#include "GeneralUtils.h"

namespace {
	constexpr auto SEND_INTERVAL = std::chrono::milliseconds(500);
	constexpr auto POSITION_INTERVAL = std::chrono::seconds(1);
	std::chrono::steady_clock::time_point g_LastPositions{};
	bool g_HadPlayers = false;
	std::set<std::pair<std::string, LWOOBJID>> g_Pending;
	std::chrono::steady_clock::time_point g_LastSend{};
}

namespace DashboardNotify {
	void Changed(const std::string& table, LWOOBJID id) {
		if (g_Pending.size() >= DataChanged::MAX_ENTRIES) return; // a burst this big reaches the dashboard's own check anyway
		g_Pending.emplace(table, id);
	}

	void Flush(bool force) {
		if (g_Pending.empty() || !Game::server) return;
		const auto now = std::chrono::steady_clock::now();
		if (!force && now - g_LastSend < SEND_INTERVAL) return;
		g_LastSend = now;

		DataChanged message;
		for (const auto& [table, id] : g_Pending) message.entries.push_back({ table, id });
		g_Pending.clear();

		CBITSTREAM;
		BitStreamUtils::WriteHeader(bitStream, ServiceType::MASTER, MessageType::Master::DATA_CHANGED);
		message.Serialize(bitStream);
		Game::server->SendToMaster(bitStream);
	}
}

namespace DashboardNotify {
	void SendPlayerPositions(uint32_t instanceId) {
		if (!Game::server || !Game::zoneManager) return;
		const auto now = std::chrono::steady_clock::now();
		if (now - g_LastPositions < POSITION_INTERVAL) return;
		g_LastPositions = now;

		const auto& players = PlayerManager::GetAllPlayers();
		// Once more after the last player leaves, so the dashboard clears this world
		if (players.empty() && !g_HadPlayers) return;
		g_HadPlayers = !players.empty();

		PlayerPositions message;
		const auto zone = Game::zoneManager->GetZoneID();
		message.zoneId = zone.GetMapID();
		message.instanceId = instanceId;
		message.cloneId = zone.GetCloneID();
		for (const auto* player : players) {
			const auto* character = player ? player->GetCharacter() : nullptr;
			if (!character) continue;
			const auto position = player->GetPosition();
			message.players.push_back({ character->GetID(), position.x, position.y, position.z });
		}

		CBITSTREAM;
		BitStreamUtils::WriteHeader(bitStream, ServiceType::MASTER, MessageType::Master::PLAYER_POSITIONS);
		message.Serialize(bitStream);
		Game::server->SendToMaster(bitStream);
	}

	std::string ChatLine(const std::string& title, const std::string& message) {
		if (title.empty()) return message;
		const char last = title.back();
		const bool punctuated = last == '!' || last == '?' || last == '.' || last == ':';
		return title + (punctuated ? " " : ": ") + message;
	}

	void Announce(const std::string& title, const std::string& message) {
		AMFArrayValue args;
		args.Insert("title", title);
		args.Insert("message", message);
		GameMessages::SendUIMessageServerToAllClients("ToggleAnnounce", args);
		// Also in chat, for anyone who closes the popup without reading it
		const auto text = GeneralUtils::UTF8ToUTF16(ChatLine(title, message));
		for (const auto* player : PlayerManager::GetAllPlayers()) {
			if (player) ChatPackets::SendSystemMessage(player->GetSystemAddress(), text);
		}
	}
}
