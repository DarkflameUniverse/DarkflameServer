#include "ChatMatchmaking.h"

#include <map>

#include "ChatPacketHandler.h"
#include "ChatPackets.h"
#include "ClientPackets.h"
#include "Game.h"
#include "GeneralUtils.h"
#include "Logger.h"
#include "PlayerContainer.h"
#include "TeamContainer.h"
#include "ZoneInstanceManager.h"
#include "dServer.h"

namespace {
	Matchmaking::Lobbies g_Lobbies;

	void SendUpdates(const std::vector<Matchmaking::Update>& updates) {
		for (const auto& update : updates) {
			const auto& player = Game::playerContainer.GetPlayerData(update.to);
			if (!player) continue;
			ClientPackets::MatchUpdate msg;
			msg.target = update.to;
			msg.data = update.data;
			msg.type = update.type;
			ChatPacketHandler::SendRouted(update.to, player.worldServerSysAddr, msg);
		}
	}

	// The players' worlds send them to the instance master started
	void SendToInstance(const Matchmaking::Match& match, const bool mythranShift, const uint32_t zoneID, const uint32_t zoneInstance, const uint32_t zoneClone, const std::string& serverIP, const uint16_t serverPort) {
		std::map<SystemAddress, ChatPackets::MatchTransfer> perWorld;
		std::vector<LWOOBJID> online;
		for (const auto playerID : match.players) {
			const auto& player = Game::playerContainer.GetPlayerData(playerID);
			if (!player || Game::playerContainer.PlayerBeingRemoved(playerID)) continue;
			online.push_back(playerID);

			auto& transfer = perWorld[player.worldServerSysAddr];
			transfer.activityID = match.settings.activityID;
			transfer.zoneID = LWOZONEID(zoneID, zoneInstance, zoneClone);
			transfer.serverIP = serverIP;
			transfer.serverPort = serverPort;
			transfer.mythranShift = mythranShift;
			transfer.players.push_back(playerID);
		}

		for (const auto& [world, transfer] : perWorld) {
			RakNet::BitStream bitStream;
			transfer.WritePacket(bitStream);
			Game::server->Send(bitStream, world, false);
		}

		// Players who start an activity together are a team, as when a world made the lobby (teams of up to
		// ChatPackets::CreateTeam::MAX_MEMBERS players; bigger groups never were one)
		if (online.size() > 1 && online.size() <= ChatPackets::CreateTeam::MAX_MEMBERS) {
			ChatPackets::CreateTeam team;
			team.leaderID = online.front();
			team.members = online;
			team.zoneID = LWOZONEID(zoneID, zoneInstance, zoneClone);
			TeamContainer::CreateTeamServer(team, UNASSIGNED_SYSTEM_ADDRESS);
		}

		LOG("Matchmaking: activity %i starts in zone %u instance %u clone %u (%zu players, %zu worlds)", match.settings.activityID, zoneID, zoneInstance, zoneClone, online.size(), perWorld.size());
	}
}

namespace ChatMatchmaking {
	Matchmaking::Lobbies& GetLobbies() {
		return g_Lobbies;
	}

	void HandleMatchRequest(const ChatPackets::MatchRequest& request, const SystemAddress& sysAddr) {
		const auto& player = Game::playerContainer.GetPlayerData(request.playerID);
		// Only players this chat server knows are in the world that sent it
		if (!player || player.worldServerSysAddr != sysAddr) return;

		std::vector<Matchmaking::Update> updates;
		switch (request.type) {
		case ChatPackets::eMatchRequestType::JOIN: {
			Matchmaking::ActivitySettings settings;
			settings.activityID = request.activityID;
			settings.instanceMapID = request.instanceMapID;
			settings.minTeams = request.minTeams;
			settings.maxTeams = request.maxTeams;
			settings.minTeamSize = request.minTeamSize;
			settings.maxTeamSize = request.maxTeamSize;
			settings.waitTime = static_cast<float>(request.waitTime) / 1000.0f;
			settings.startDelay = static_cast<float>(request.startDelay) / 1000.0f;
			const auto& name = request.playerName.empty() ? player.playerName : request.playerName;
			g_Lobbies.Join(request.playerID, name, request.playerChoices, settings, updates);
			break;
		}
		case ChatPackets::eMatchRequestType::READY:
			g_Lobbies.SetReady(request.playerID, request.value != 0, updates);
			break;
		case ChatPackets::eMatchRequestType::LEAVE:
			g_Lobbies.Leave(request.playerID, updates);
			break;
		}
		SendUpdates(updates);
	}

	void PlayerLeftWorld(const LWOOBJID playerID) {
		std::vector<Matchmaking::Update> updates;
		if (g_Lobbies.Leave(playerID, updates)) SendUpdates(updates);
	}

	void Update(const float deltaTime) {
		if (g_Lobbies.GetLobbies().empty()) return;
		std::vector<Matchmaking::Update> updates;
		const auto matches = g_Lobbies.Tick(deltaTime, updates);
		SendUpdates(updates);
		for (const auto& match : matches) StartMatch(match);
	}

	void StartMatch(const Matchmaking::Match& match) {
		// A clone no other instance has: master starts a new instance of the zone for this match alone
		const auto cloneID = GeneralUtils::GenerateRandomNumber<uint32_t>(1, UINT32_MAX);
		ZoneInstanceManager::Instance()->RequestZoneTransfer(Game::server, match.settings.instanceMapID, cloneID, false,
			[match](bool mythranShift, uint32_t zoneID, uint32_t zoneInstance, uint32_t zoneClone, std::string serverIP, uint16_t serverPort) {
				SendToInstance(match, mythranShift, zoneID, zoneInstance, zoneClone, serverIP, serverPort);
			});
	}
}
