#ifndef AUTHPACKETS_H
#define AUTHPACKETS_H

#define _VARIADIC_MAX 10
#include "dCommonVars.h"
#include "dNetCommon.h"

enum class ServerType : uint32_t;
enum class eLoginResponse : uint8_t;
class dServer;

namespace AuthPackets {
	void HandleHandshake(dServer* server, Packet* packet);
	void SendHandshake(dServer* server, const SystemAddress& sysAddr, const std::string& nextServerIP, uint16_t nextServerPort, const ServerType serverType);

	void HandleLoginRequest(dServer* server, Packet* packet);
	void SendLoginResponse(dServer* server, const SystemAddress& sysAddr, eLoginResponse responseCode, const std::string& errorMsg, const std::string& wServerIP, uint16_t wServerPort, std::string username);
}

#endif // AUTHPACKETS_H
