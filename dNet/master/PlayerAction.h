#ifndef __PLAYERACTION__H__
#define __PLAYERACTION__H__

#include <algorithm>
#include <cstdint>
#include <string>

#include "BitStream.h"
#include "BitStreamUtils.h"
#include "MessageType/Master.h"
#include "dCommonVars.h"

/**
 * Actions the dashboard asks world servers to apply to players who are currently online.
 * The dashboard sends MessageType::Master::PLAYER_ACTION to master, which forwards it to every world.
 * Each world answers PLAYER_ACTION_RESULT, and master sends the dashboard a single aggregated result
 * once every world has answered or the request timed out.
 */
enum class ePlayerAction : uint8_t {
	KICK_ACCOUNT,       // Disconnect every session of accountId
	REFRESH_ACCOUNT,    // Reload account data (GM level) for online sessions of accountId
	REFRESH_CHARACTER,  // Reload character data (restrictions) for characterId if loaded
	RESCUE_CHARACTER,   // Transfer characterId to zoneId if it is loaded in a world, landing on spawn point `text` (empty: the zone's default)
	// Tell a player a moderator decided on something they asked for (approved = yes/no, text = name or reason)
	NAME_MODERATED,     // characterId's requested name; text is the name
	PET_NAME_MODERATED, // pet targetId's name; text is the name
	PROPERTY_MODERATED, // property targetId owned by characterId; text is the rejection reason
	TELEPORT_TO_PLAYER, // move characterId to where character targetId stands (only when both are in the same world)
	WARN_ACCOUNT,       // show accountId's online character a moderator's warning (text)
	RELOAD_VANITY,      // respawn the vanity NPCs from the vanity/*.xml files (every world; answers 1 per world reloaded)
	CHAT_MESSAGE,       // show `text` in chat from `name` (e.g. "[Discord] Bob") in every world, or only zoneId; answers players reached
	RELOAD_CHAT_FILTER, // load the chat filter's words added on the dashboard again (every world, answering 1 each; master also passes it to the chat server)
	// Change mission targetId of characterId if it is loaded in a world, as the GM commands do
	MISSION_COMPLETE,   // as /completemission; approved = give its rewards
	MISSION_RESET,      // as /resetmission
	MISSION_ACCEPT,     // as /addmission (prerequisites are skipped)
	RELOAD_LIVE_OPS,    // load the running live events and open challenges again (every world, answering 1 each; see LiveEvents.h)
	RELOAD_CONTRABAND,  // load the contraband list again (every world, answering 1 each; see Contraband.h)
};

struct PlayerActionRequest : public LUBitStream {
	PlayerActionRequest() : LUBitStream(ServiceType::MASTER, MessageType::Master::PLAYER_ACTION) {}

	uint32_t requestId{};
	ePlayerAction action{};
	uint32_t accountId{};
	LWOOBJID characterId{};
	LWOMAPID zoneId{};
	// eServerDisconnectIdentifiers shown to the client for KICK_ACCOUNT
	uint32_t disconnectReason{};
	// For the *_MODERATED actions
	LWOOBJID targetId{};
	bool approved{};
	std::string text;
	std::string name; // CHAT_MESSAGE: who it's from
	int32_t instanceId{ -1 }; // CHAT_MESSAGE: only this instance of zoneId (-1: every instance)

	static constexpr uint16_t MAX_TEXT = 300;

	// How many bytes of UTF-8 text fit in maxBytes without cutting a character in half
	static size_t Utf8Length(const std::string& value, size_t maxBytes) {
		if (value.size() <= maxBytes) return value.size();
		size_t length = maxBytes;
		// Back up over continuation bytes (10xxxxxx) to the start of the character that doesn't fit
		while (length > 0 && (static_cast<unsigned char>(value[length]) & 0xC0) == 0x80) length--;
		return length;
	}

	void Serialize(RakNet::BitStream& bitStream) const override {
		bitStream.Write(requestId);
		bitStream.Write(action);
		bitStream.Write(accountId);
		bitStream.Write(characterId);
		bitStream.Write(zoneId);
		bitStream.Write(disconnectReason);
		bitStream.Write(targetId);
		bitStream.Write<uint8_t>(approved);
		const auto length = static_cast<uint16_t>(Utf8Length(text, MAX_TEXT));
		bitStream.Write(length);
		bitStream.Write(text.data(), length);
		const auto nameLength = static_cast<uint16_t>(Utf8Length(name, MAX_TEXT));
		bitStream.Write(nameLength);
		bitStream.Write(name.data(), nameLength);
		bitStream.Write(instanceId);
	}

	bool Deserialize(RakNet::BitStream& bitStream) override {
		uint8_t approvedByte{};
		uint16_t length{};
		if (!(bitStream.Read(requestId) && bitStream.Read(action) && bitStream.Read(accountId) && bitStream.Read(characterId) &&
			bitStream.Read(zoneId) && bitStream.Read(disconnectReason) && bitStream.Read(targetId) && bitStream.Read(approvedByte) &&
			bitStream.Read(length)) || length > MAX_TEXT) return false;
		approved = approvedByte != 0;
		text.resize(length);
		if (length != 0 && !bitStream.Read(text.data(), length)) return false;
		uint16_t nameLength{};
		if (!bitStream.Read(nameLength) || nameLength > MAX_TEXT) return false;
		name.resize(nameLength);
		if (nameLength != 0 && !bitStream.Read(name.data(), nameLength)) return false;
		return bitStream.Read(instanceId);
	}
};

struct PlayerActionResult : public LUBitStream {
	PlayerActionResult() : LUBitStream(ServiceType::MASTER, MessageType::Master::PLAYER_ACTION_RESULT) {}

	uint32_t requestId{};
	ePlayerAction action{};
	// Number of sessions/characters the action applied to. Zero means the player was not online.
	uint32_t affected{};
	// Set by master when not every world answered before the timeout
	bool timedOut{};

	void Serialize(RakNet::BitStream& bitStream) const override {
		bitStream.Write(requestId);
		bitStream.Write(action);
		bitStream.Write(affected);
		bitStream.Write<uint8_t>(timedOut);
	}

	bool Deserialize(RakNet::BitStream& bitStream) override {
		uint8_t timedOutByte{};
		const bool ok = bitStream.Read(requestId) && bitStream.Read(action) && bitStream.Read(affected) && bitStream.Read(timedOutByte);
		timedOut = timedOutByte != 0;
		return ok;
	}
};

#endif  //!__PLAYERACTION__H__
