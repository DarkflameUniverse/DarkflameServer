#ifndef AUTHPACKETS_H
#define AUTHPACKETS_H

#define _VARIADIC_MAX 10
#include "dCommonVars.h"
#include "dNetCommon.h"

enum class ServerType : uint32_t;
enum class eLoginResponse : uint8_t;
enum class eStamps : uint32_t;
class dServer;

struct Stamp {
	eStamps type;
	uint32_t value;
	uint64_t timestamp;

	Stamp(eStamps type, uint32_t value, uint64_t timestamp){
		this->type = type;
		this->value = value;
		this->timestamp = timestamp;
	}

	void Serialize(RakNet::BitStream* outBitStream);
};

namespace AuthPackets {
	void HandleHandshake(dServer* server, Packet* packet);
	void SendHandshake(dServer* server, const SystemAddress& sysAddr, const std::string& nextServerIP, uint16_t nextServerPort, const ServerType serverType);

	void HandleLoginRequest(dServer* server, Packet* packet);
	void SendLoginResponse(dServer* server, const SystemAddress& sysAddr, eLoginResponse responseCode, const std::string& errorMsg, const std::string& wServerIP, uint16_t wServerPort, std::string username);
}

#endif // AUTHPACKETS_H
