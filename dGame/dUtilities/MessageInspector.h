#ifndef __MESSAGEINSPECTOR__H__
#define __MESSAGEINSPECTOR__H__

#include <cstdint>

#include "dCommonVars.h"
#include "MessageType/Game.h"

namespace RakNet { class BitStream; }
struct MessageCaptureControl;
struct SystemAddress;

/**
 * This world's side of the dashboard's game message inspector (see MessageCapture.h): captures the game messages
 * one player sends and receives and sends them to the dashboard through master in small, rate-limited batches.
 *
 * Messages are seen at the two places every game message passes: GameMessageHandler::HandleMessage for what
 * clients send, and dServer::Send for what the server sends (hooked only while a capture runs). With no capture
 * running, the only cost is the IsCapturing() check on received messages.
 */
namespace MessageInspector {
	extern bool g_Capturing;

	inline bool IsCapturing() { return g_Capturing; }

	// A start or stop from the dashboard. Starts only if the character is in this world.
	void Control(const MessageCaptureControl& control);

	// A game message a client sent (payload: the bits after the object ID and message ID)
	void RecordReceived(const SystemAddress& sysAddr, LWOOBJID objectId, MessageType::Game messageId, const RakNet::BitStream& payload);

	// Called from the world server loop: sends what was captured and ends captures that ran out or lost their player
	void Update();
}

#endif  //!__MESSAGEINSPECTOR__H__
