#ifndef AUTHPACKETS_H
#define AUTHPACKETS_H

#define _VARIADIC_MAX 10
#include "dCommonVars.h"
#include "dNetCommon.h"
#include "magic_enum.hpp"

enum class ServerType : uint32_t;
enum class eLoginResponse : uint8_t;
class dServer;

enum class eStamps : uint32_t {
	START,
	BYPASS,
	ERROR,
	DB_SELECT_START,
	DB_SELECT_FINISH,
	DB_INSERT_START,
	DB_INSERT_FINISH,
	LEGOINT_COMMUNICATION_START,
	LEGOINT_RECEIVED,
	LEGOINT_THREAD_SPAWN,
	LEGOINT_WEBSERVICE_START,
	LEGOINT_WEBSERVICE_FINISH,
	LEGOINT_LEGOCLUB_START,
	LEGOINT_LEGOCLUB_FINISH,
	LEGOINT_THREAD_FINISH,
	LEGOINT_REPLY,
	LEGOINT_ERROR,
	LEGOINT_COMMUNICATION_END,
	LEGOINT_DISCONNECT,
	WORLD_COMMUNICATION_START,
	CLIENT_OS,
	WORLD_PACKET_RECEIVED,
	IM_COMMUNICATION_START,
	IM_LOGIN_START,
	IM_LOGIN_ALREADY_LOGGED_IN,
	IM_OTHER_LOGIN_REMOVED,
	IM_LOGIN_QUEUED,
	IM_LOGIN_RESPONSE,
	IM_COMMUNICATION_END,
	WORLD_SESSION_CONFIRM_TO_AUTH,
	WORLD_COMMUNICATION_FINISH,
	WORLD_DISCONNECT,
	NO_LEGO_INTERFACE,
	DB_ERROR,
	GM_REQUIRED,
	NO_LEGO_WEBSERVICE_XML,
	LEGO_WEBSERVICE_TIMEOUT,
	LEGO_WEBSERVICE_ERROR,
	NO_WORLD_SERVER
};

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
	de_DE = 0x0407,
	en_US = 0x0409,
	en_GB = 0x0809
};

template <>
struct magic_enum::customize::enum_range<LanguageCodeID> {
	static constexpr int min = 1031;
	static constexpr int max = 2057;
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
	void SendLoginResponse(dServer* server, const SystemAddress& sysAddr, eLoginResponse responseCode, const std::string& errorMsg, const std::string& wServerIP, uint16_t wServerPort, std::string username, std::vector<Stamp>& stamps);
	void LoadClaimCodes();

}

#endif // AUTHPACKETS_H
