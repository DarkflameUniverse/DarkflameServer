/*
 * Darkflame Universe
 * Copyright 2018
 */

#ifndef GAMEMESSAGEHANDLER_H
#define GAMEMESSAGEHANDLER_H

#include "RakNetTypes.h"
#include "dCommonVars.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <unordered_map>
#include "BitStream.h"
#include "Entity.h"
#include "EntityManager.h"
#include "Game.h"
#include "Logger.h"
#include "GameMessages.h"
#include "CDClientDatabase.h"
#include "MessageType/Game.h"

namespace GameMessageHandler {
	void HandleMessage(RakNet::BitStream& inStream, const SystemAddress& sysAddr, LWOOBJID objectID, MessageType::Game messageID);

	// A new, empty typed struct for a message a client sends, or nullptr when the server reads it inline (capture
	// fixtures read recorded messages with it)
	std::unique_ptr<GameMessages::NetGameMsg> CreateReceived(MessageType::Game messageID);
};

#endif // GAMEMESSAGEHANDLER_H
