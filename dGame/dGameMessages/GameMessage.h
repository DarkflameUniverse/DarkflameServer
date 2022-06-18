#pragma once

#include "dCommonVars.h"
#include "dMessageIdentifiers.h"
#include "Entity.h"

class GameMessage {
public:
	constexpr static GAME_MSG GetId() { return GAME_MSG_INVALID; }

	SystemAddress sysAddr;
	Entity* associate;

	virtual void Deserialize(RakNet::BitStream* inStream) {};
    virtual void Serialize(RakNet::BitStream* outStream) {};
	virtual void Handle() {};
};