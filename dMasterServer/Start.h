#pragma once
#include "dCommonVars.h"

void StartAuthServer();
void StartChatServer();
void StartWorldServer(LWOMAPID mapID, uint16_t port, LWOINSTANCEID lastInstanceID, int maxPlayers, LWOCLONEID cloneID);
