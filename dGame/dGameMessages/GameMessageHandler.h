/*
 * Darkflame Universe
 * Copyright 2018
 */

#ifndef GAMEMESSAGEHANDLER_H
#define GAMEMESSAGEHANDLER_H

#include "RakNetTypes.h"
#include "dMessageIdentifiers.h"
#include "dCommonVars.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <unordered_map>
#include "BitStream.h"
#include "Entity.h"
#include "EntityManager.h"
#include "Game.h"
#include "dLogger.h"
#include "GameMessages.h"
#include "../dDatabase/CDClientDatabase.h"

namespace GameMessageHandler {
	void HandleMessage(RakNet::BitStream* inStream, const SystemAddress& sysAddr, LWOOBJID objectID, GAME_MSG messageID);
};

#endif // GAMEMESSAGEHANDLER_H
