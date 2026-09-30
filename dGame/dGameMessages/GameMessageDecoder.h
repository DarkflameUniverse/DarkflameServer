#ifndef __GAMEMESSAGEDECODER__H__
#define __GAMEMESSAGEDECODER__H__

#include <optional>
#include <vector>

#include "BitStream.h"
#include "json.hpp"
#include "MessageType/Game.h"

/**
 * Reads a game message's fields for the dashboard's message inspector and capture viewer, using the server's own
 * message struct and its Deserialize, so what is shown is exactly what the server reads. Every wire message struct
 * (NetGameMsg in *Messages.h) is registered; its members are listed by name by GameMessageFields.inc, which
 * tools/gen_game_message_fields.py writes from the struct definitions. Messages the server has no struct for aren't
 * decoded; the viewer shows their name and bytes.
 */
namespace GameMessageDecoder {
	// Whether a message sent in this direction has a struct that reads it
	bool CanDecode(MessageType::Game messageId, bool toServer);

	// Whether the server has a struct for the message at all (in either direction)
	bool HasStruct(MessageType::Game messageId);

	// The message's fields, read from `payload` (the bits after the object ID and message ID), or nullopt when it has
	// no struct or doesn't read cleanly. Whole bytes the struct left unread are reported as "(unread bits)".
	std::optional<nlohmann::json> Decode(MessageType::Game messageId, bool toServer, RakNet::BitStream& payload);

	// Every message ID with a struct that reads it
	std::vector<MessageType::Game> Decodable();

	/**
	 * Reads a message a client sent with the struct the server reads it with, and writes it again. nullopt when the
	 * server has no typed struct for it or it doesn't read; otherwise whether it read the whole message (padding
	 * after the last field aside) and the same bits came back. The capture fixture tests run it on every recorded client message.
	 */
	std::optional<bool> RoundTripReceived(MessageType::Game messageId, RakNet::BitStream& payload);
}

#endif  //!__GAMEMESSAGEDECODER__H__
