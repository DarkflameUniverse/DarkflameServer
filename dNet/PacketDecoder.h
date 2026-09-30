#ifndef __PACKETDECODER__H__
#define __PACKETDECODER__H__

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

#include "BitStream.h"
#include "dCommonVars.h"
#include "json.hpp"
#include "MessageType/Game.h"
#include "NiPoint3.h"

enum class ServiceType : uint16_t;

/**
 * Reads recorded packets (PacketRecord.h) for the dashboard's capture viewer and the capture tool, with the
 * server's own packet structs and their Deserialize: a registry keyed by (service, message ID). Every packet gets a
 * name (RakNet's own messages, LU packets by their MessageType enum); the ones registered here also get their fields.
 * Game messages (WORLD and CLIENT GAME_MSG) are named by MessageType::Game; their fields come from the decoder a
 * program that links the game sets with SetGameMessageDecoder (the capture tool does; the dashboard doesn't link
 * the game, so it shows their bytes).
 */
namespace PacketDecoder {
	struct Decoded {
		std::string service;      // "WORLD", "CLIENT", ... or "RAKNET" for RakNet's own messages
		std::string name;         // e.g. "POSITION_UPDATE", "GAME_MSG REQUEST_USE", "ID_REPLICA_MANAGER_CONSTRUCTION"
		uint16_t serviceId{};
		uint32_t messageId{};     // the LU message ID, or the RakNet message ID
		bool lu{};                // an LU packet (8 byte header)
		int32_t gameMessageId{ -1 };
		LWOOBJID objectId{};      // game messages: the object it is for
		std::optional<nlohmann::json> fields; // set when a registered struct read it cleanly
		bool failed{};            // a registered struct did not read it
	};

	// `fromClient`: the packet came from a game client (worlds and auth receiving on their listening peer)
	Decoded Decode(std::string_view bytes, bool fromClient);

	// The name alone
	std::string Name(ServiceType service, uint32_t messageId);

	// Where a client's POSITION_UPDATE says the player is; nullopt for any other packet
	std::optional<NiPoint3> Position(std::string_view bytes);

	using GameMessageFields = std::function<std::optional<nlohmann::json>(MessageType::Game messageId, bool toServer, RakNet::BitStream& payload)>;
	void SetGameMessageDecoder(GameMessageFields decoder);

	/**
	 * Secrets never reach a capture: packets whose struct declares secret fields (passwords, session and user keys)
	 * are read, those fields blanked (strings emptied, keys 0), and written again before they are recorded. Returns
	 * false when the packet declares secrets but doesn't read cleanly: then it must not be recorded at all.
	 * Packets without secrets are left as they are.
	 */
	bool Redact(std::string& bytes);

	/**
	 * For bundles that leave the server: account names are always replaced (portable bundles), and with `anonymise`
	 * also character names and what players typed (test fixtures). Text is replaced with as many 'x' as it had, so
	 * the packet keeps its size. Returns true if the packet changed.
	 */
	bool Scrub(std::string& bytes, bool anonymise);

	// Whether a packet's struct declares secret fields (they are blanked when recorded)
	bool HasSecrets(ServiceType service, uint32_t messageId);

	/**
	 * Reads a packet with its struct and writes it again. nullopt: no struct for it (or it doesn't read); otherwise
	 * whether the bytes came back the same (the local fixture tests' byte-equality check).
	 */
	std::optional<bool> RoundTrip(std::string_view bytes);

	// How many (service, message) pairs have typed decoders, for tests
	size_t RegisteredCount();

	// Whether a (service, message) has a struct its fields are read with
	bool HasFields(ServiceType service, uint32_t messageId);
}

#endif  //!__PACKETDECODER__H__
