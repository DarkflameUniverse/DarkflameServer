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

enum class ClientOS : uint8_t {
	UNKNOWN,
	WINDOWS,
	MACOS
};

enum class LanguageCodeID : uint16_t {
	en_US = 0x0409,
	de_DE = 0x0407,
	en_GB = 0x0809
};

enum class Language : uint32_t {
	en_US,
	pl_US,
	de_DE,
	en_GB,
};

namespace AuthPackets {
	void HandleHandshake(dServer* server, Packet* packet);
	void SendHandshake(dServer* server, const SystemAddress& sysAddr, const std::string& nextServerIP, uint16_t nextServerPort, const ServerType serverType);
	void HandleLoginRequest(dServer* server, Packet* packet);
	void SendLoginResponse(dServer* server, const SystemAddress& sysAddr, eLoginResponse responseCode, const std::string& errorMsg, const std::string& wServerIP, uint16_t wServerPort, std::string username);
}

#endif // AUTHPACKETS_H
