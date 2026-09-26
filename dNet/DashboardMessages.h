#ifndef __DASHBOARDMESSAGES__H__
#define __DASHBOARDMESSAGES__H__

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "BitStream.h"
#include "dCommonVars.h"

namespace DashboardMessages {
	// Length-prefixed string, capped so a bad packet can't ask for huge allocations
	inline void WriteText(RakNet::BitStream& stream, const std::string& text, uint16_t max) {
		const auto length = static_cast<uint16_t>(std::min<size_t>(text.size(), max));
		stream.Write(length);
		stream.Write(text.data(), length);
	}

	inline bool ReadText(RakNet::BitStream& stream, std::string& text, uint16_t max) {
		uint16_t length{};
		if (!stream.Read(length) || length > max) return false;
		text.resize(length);
		return length == 0 || stream.Read(text.data(), length);
	}
}

/**
 * PLAYER_POSITIONS (world -> master -> dashboard): where each player in a world instance is, sent every
 * second while anyone is online and once more when the world empties.
 */
struct PlayerPositions {
	struct Player {
		LWOOBJID characterId{};
		float x{}, y{}, z{};
	};

	static constexpr uint16_t MAX_PLAYERS = 1000;

	uint32_t zoneId{};
	uint32_t instanceId{};
	uint32_t cloneId{};
	std::vector<Player> players;

	void Serialize(RakNet::BitStream& stream) const {
		stream.Write(zoneId);
		stream.Write(instanceId);
		stream.Write(cloneId);
		const auto count = static_cast<uint16_t>(std::min<size_t>(players.size(), MAX_PLAYERS));
		stream.Write(count);
		for (uint16_t i = 0; i < count; i++) {
			stream.Write(players[i].characterId);
			stream.Write(players[i].x);
			stream.Write(players[i].y);
			stream.Write(players[i].z);
		}
	}

	bool Deserialize(RakNet::BitStream& stream) {
		uint16_t count{};
		if (!stream.Read(zoneId) || !stream.Read(instanceId) || !stream.Read(cloneId) || !stream.Read(count) || count > MAX_PLAYERS) return false;
		players.resize(count);
		for (auto& player : players) {
			if (!stream.Read(player.characterId) || !stream.Read(player.x) || !stream.Read(player.y) || !stream.Read(player.z)) return false;
		}
		return true;
	}
};

/**
 * ANNOUNCE (dashboard -> master -> worlds): a message shown to every player online, or only to those in the listed
 * zones (master only forwards it to worlds of those zones).
 */
struct Announcement {
	static constexpr uint16_t MAX_TITLE = 100;
	static constexpr uint16_t MAX_MESSAGE = 1000;
	static constexpr uint16_t MAX_ZONES = 200;

	std::string title;
	std::string message;
	std::vector<uint32_t> zones; // empty: every world

	void Serialize(RakNet::BitStream& stream) const {
		DashboardMessages::WriteText(stream, title, MAX_TITLE);
		DashboardMessages::WriteText(stream, message, MAX_MESSAGE);
		const auto count = static_cast<uint16_t>(std::min<size_t>(zones.size(), MAX_ZONES));
		stream.Write(count);
		for (uint16_t i = 0; i < count; i++) stream.Write(zones[i]);
	}

	bool Deserialize(RakNet::BitStream& stream) {
		if (!DashboardMessages::ReadText(stream, title, MAX_TITLE) || !DashboardMessages::ReadText(stream, message, MAX_MESSAGE)) return false;
		uint16_t count{};
		if (!stream.Read(count) || count > MAX_ZONES) return false;
		zones.resize(count);
		for (auto& zone : zones) if (!stream.Read(zone)) return false;
		return true;
	}

	bool ShownIn(uint32_t zoneId) const {
		return zones.empty() || std::find(zones.begin(), zones.end(), zoneId) != zones.end();
	}
};

#endif  //!__DASHBOARDMESSAGES__H__
