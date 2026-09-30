#include "Matchmaking.h"

#include <algorithm>
#include <ranges>

#include "GeneralUtils.h"
#include "LDFFormat.h"

namespace Matchmaking {
	uint32_t ActivitySettings::Capacity() const {
		return static_cast<uint32_t>(std::max(maxTeamSize == 1 ? maxTeams : maxTeamSize, 1));
	}

	uint32_t ActivitySettings::Minimum() const {
		return static_cast<uint32_t>(std::max(maxTeamSize == 1 ? minTeams : minTeamSize, 1));
	}

	std::string PlayerText(const LWOOBJID player) {
		return LDFData<LWOOBJID>(u"player", player).GetString();
	}

	namespace {
		// The racing car the client names in its MatchRequest ("droppedItem=13:<id>", its text); live showed it to the
		// lobby as an object ID ("droppedItem=9:<id>") before the player's line
		std::string DroppedItemText(const std::string& choices) {
			for (const auto& line : GeneralUtils::SplitString(choices, '\n')) {
				const auto equals = line.find('=');
				if (equals == std::string::npos || line.substr(0, equals) != "droppedItem") continue;
				const auto colon = line.find(':', equals);
				if (colon == std::string::npos) continue;
				const auto item = GeneralUtils::TryParse<LWOOBJID>(line.substr(colon + 1));
				if (!item) continue;
				return LDFData<LWOOBJID>(u"droppedItem", *item).GetString() + "\n";
			}
			return "";
		}
	}

	// The name as a wide string (type 0), as live sent it
	std::string PlayerAddedText(const LobbyPlayer& player) {
		return DroppedItemText(player.choices) + PlayerText(player.id) + "\n" + LDFData<std::u16string>(u"playerName", GeneralUtils::UTF8ToUTF16(player.name)).GetString();
	}

	std::string TimeText(const float seconds) {
		return LDFData<float>(u"time", seconds).GetString();
	}

	void Lobbies::Broadcast(const Lobby& lobby, const eMatchUpdate type, const std::string& data, std::vector<Update>& out) {
		for (const auto& player : lobby.players) out.push_back({ player.id, type, data });
	}

	const Lobby* Lobbies::Find(const LWOOBJID player) const {
		for (const auto& lobby : m_Lobbies | std::views::values) {
			for (const auto& lobbyPlayer : lobby.players) {
				if (lobbyPlayer.id == player) return &lobby;
			}
		}
		return nullptr;
	}

	Lobby* Lobbies::FindMutable(const LWOOBJID player) {
		return const_cast<Lobby*>(Find(player));
	}

	bool Lobbies::Join(const LWOOBJID player, const std::string& name, const std::string& choices, const ActivitySettings& settings, std::vector<Update>& out) {
		if (const auto* current = Find(player)) {
			if (current->settings.activityID == settings.activityID && current->settings.instanceMapID == settings.instanceMapID) return false;
			// Waiting for something else: that lobby loses them
			Leave(player, out);
		}

		Lobby* lobby = nullptr;
		for (auto& candidate : m_Lobbies | std::views::values) {
			if (candidate.settings.activityID == settings.activityID && candidate.settings.instanceMapID == settings.instanceMapID
				&& candidate.players.size() < candidate.settings.Capacity()) {
				lobby = &candidate;
				break;
			}
		}
		if (!lobby) {
			lobby = &m_Lobbies[m_NextLobbyID++];
			lobby->settings = settings;
			lobby->timer = settings.waitTime;
		}

		const LobbyPlayer joining{ player, name, choices, false };
		const auto joinedText = PlayerAddedText(joining);

		// The joining player: themselves first, then everyone already waiting and whether they are ready (live order)
		out.push_back({ player, eMatchUpdate::PLAYER_ADDED, joinedText });
		for (const auto& waiting : lobby->players) {
			out.push_back({ player, eMatchUpdate::PLAYER_ADDED, PlayerAddedText(waiting) });
			if (waiting.ready) out.push_back({ player, eMatchUpdate::PLAYER_READY, PlayerText(waiting.id) });
		}
		// Everyone already waiting: the new player
		Broadcast(*lobby, eMatchUpdate::PLAYER_ADDED, joinedText, out);

		lobby->players.push_back(joining);

		// A countdown already running: the new player gets the time left
		if (lobby->counting) out.push_back({ player, eMatchUpdate::PHASE_WAIT_READY, TimeText(lobby->timer) });
		return true;
	}

	bool Lobbies::Leave(const LWOOBJID player, std::vector<Update>& out) {
		for (auto it = m_Lobbies.begin(); it != m_Lobbies.end(); ++it) {
			auto& players = it->second.players;
			const auto found = std::ranges::find(players, player, &LobbyPlayer::id);
			if (found == players.end()) continue;

			// Everyone in the lobby, the one leaving included
			Broadcast(it->second, eMatchUpdate::PLAYER_REMOVED, PlayerText(player), out);
			players.erase(found);
			if (players.empty()) m_Lobbies.erase(it);
			return true;
		}
		return false;
	}

	bool Lobbies::SetReady(const LWOOBJID player, const bool ready, std::vector<Update>& out) {
		auto* lobby = FindMutable(player);
		if (!lobby) return false;

		std::ranges::find(lobby->players, player, &LobbyPlayer::id)->ready = ready;
		Broadcast(*lobby, ready ? eMatchUpdate::PLAYER_READY : eMatchUpdate::PLAYER_NOT_READY, PlayerText(player), out);
		return true;
	}

	std::vector<Match> Lobbies::Tick(const float deltaTime, std::vector<Update>& out) {
		std::vector<Match> matches;
		for (auto it = m_Lobbies.begin(); it != m_Lobbies.end();) {
			auto& lobby = it->second;
			if (lobby.players.empty()) {
				it = m_Lobbies.erase(it);
				continue;
			}

			// The countdown runs only while the lobby has enough players; it doesn't start over when one leaves
			if (lobby.players.size() >= lobby.settings.Minimum()) {
				if (!lobby.counting) {
					lobby.counting = true;
					Broadcast(lobby, eMatchUpdate::PHASE_WAIT_READY, TimeText(lobby.timer), out);
				}
				lobby.timer -= deltaTime;

				// Everyone ready: only the start delay is left
				const bool allReady = std::ranges::all_of(lobby.players, &LobbyPlayer::ready);
				if (allReady && lobby.timer > lobby.settings.startDelay) {
					lobby.timer = lobby.settings.startDelay;
					Broadcast(lobby, eMatchUpdate::PHASE_WAIT_START, TimeText(lobby.timer), out);
				}

				if (lobby.timer <= 0.0f) {
					Match match{ lobby.settings, {} };
					for (const auto& player : lobby.players) match.players.push_back(player.id);
					matches.push_back(std::move(match));
					it = m_Lobbies.erase(it);
					continue;
				}
			}
			++it;
		}
		return matches;
	}
}
