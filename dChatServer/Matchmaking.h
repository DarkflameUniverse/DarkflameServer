#ifndef MATCHMAKING_H
#define MATCHMAKING_H

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "dCommonVars.h"
#include "eMatchUpdate.h"

/**
 * Activity lobbies of every world, kept by the chat server (docs/Matchmaking.md). Players on different instances of a
 * zone who join the same activity wait in the same lobby and are sent to the same activity instance.
 *
 * Only the state machine: no network, no game data. What it does is handed back as MatchUpdates to send to clients
 * and Matches to start; ChatMatchmaking sends them.
 */
namespace Matchmaking {
	// What the world read from the activity (CDClient Activities, with the world's own overrides)
	struct ActivitySettings {
		int32_t activityID{};
		uint32_t instanceMapID{};
		int32_t minTeams{};
		int32_t maxTeams{};
		int32_t minTeamSize{};
		int32_t maxTeamSize{};
		float waitTime{};   // seconds
		float startDelay{}; // seconds

		// How many players a lobby holds: the team size, or the number of teams when every team is one player
		uint32_t Capacity() const;
		// How many players a lobby needs before its countdown runs
		uint32_t Minimum() const;
	};

	// A game message MatchUpdate for one client
	struct Update {
		LWOOBJID to{};
		eMatchUpdate type{};
		std::string data; // name-value text
	};

	// A lobby whose countdown ran out: these players go to one new instance of the activity's zone
	struct Match {
		ActivitySettings settings;
		std::vector<LWOOBJID> players; // in the order they joined
	};

	struct LobbyPlayer {
		LWOOBJID id{};
		std::string name;
		std::string choices; // the client's name-value text, shown to the others with the player
		bool ready{};
	};

	struct Lobby {
		ActivitySettings settings;
		std::vector<LobbyPlayer> players;
		float timer{};        // seconds left
		bool counting{};      // the lobby reached its minimum and everyone was told the countdown
	};

	class Lobbies {
	public:
		// A player joins the activity: they go into the first lobby of it with room, or a new one.
		// Returns false when they are already waiting for this activity (nothing changes).
		bool Join(LWOOBJID player, const std::string& name, const std::string& choices, const ActivitySettings& settings, std::vector<Update>& out);

		// The player leaves the lobby they are in. Returns false when they are in none.
		bool Leave(LWOOBJID player, std::vector<Update>& out);

		// The player is ready (or not). Returns false when they are in no lobby.
		bool SetReady(LWOOBJID player, bool ready, std::vector<Update>& out);

		// Runs the countdowns; returns the lobbies that start now (they are removed)
		std::vector<Match> Tick(float deltaTime, std::vector<Update>& out);

		bool IsWaiting(LWOOBJID player) const { return Find(player) != nullptr; }
		const Lobby* Find(LWOOBJID player) const;
		const std::map<uint64_t, Lobby>& GetLobbies() const { return m_Lobbies; }
		void Clear() { m_Lobbies.clear(); }

	private:
		Lobby* FindMutable(LWOOBJID player);
		static void Broadcast(const Lobby& lobby, eMatchUpdate type, const std::string& data, std::vector<Update>& out);

		// In the order they were made, so a joining player fills the oldest lobby first
		std::map<uint64_t, Lobby> m_Lobbies;
		uint64_t m_NextLobbyID = 1;
	};

	// The name-value texts of the updates, as live sent them
	std::string PlayerText(LWOOBJID player);
	std::string PlayerAddedText(const LobbyPlayer& player);
	std::string TimeText(float seconds);
}

#endif // MATCHMAKING_H
