#ifndef __GAMEMESSAGEDECODER__H__
#define __GAMEMESSAGEDECODER__H__

#include <optional>

#include "BitStream.h"
#include "json.hpp"
#include "MessageType/Game.h"

/**
 * Reads a game message's fields for the dashboard's message inspector, using the server's own typed message struct
 * and its Deserialize, so what is shown is exactly what the server reads. Messages the server has no typed struct
 * for (the ones handled by reading the stream inline) aren't decoded; the inspector shows their bytes instead.
 */
namespace GameMessageDecoder {
	// Whether a message sent in this direction has a typed struct
	bool CanDecode(MessageType::Game messageId, bool toServer);

	// The message's fields, read from `payload` (the bits after the object ID and message ID), or nullopt when it has
	// no typed struct or doesn't read cleanly
	std::optional<nlohmann::json> Decode(MessageType::Game messageId, bool toServer, RakNet::BitStream& payload);
}

#endif  //!__GAMEMESSAGEDECODER__H__
