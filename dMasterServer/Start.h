#pragma once
#include "dCommonVars.h"

uint32_t StartAuthServer();
uint32_t StartChatServer();
uint32_t StartWorldServer(LWOMAPID mapID, uint16_t port, LWOINSTANCEID lastInstanceID, int maxPlayers, LWOCLONEID cloneID);
