#ifndef STAMPS_H
#define STAMPS_H

#include "BitStream.h"

#include <cstdint>
#include <ctime>
#include <string>
#include <vector>

/**
 * Login stamps: a trace of the steps the auth server went through for one login, sent at the end of the login
 * response (ClientPackets::LoginResponse::stamps).
 *
 * On the wire: a u32 holding 16 * count + 4, then count stamps of { u32 type, u32 value, u64 timestamp }.
 *
 * What the 1.10.64 client does with them (PacketHandler_MSG_CLIENT_LOGIN_RESPONSE @ 00b32f90, which reads them
 * through LoginResponse::ReadVariableData @ 005ed4f0 and LoginResponse::ReadStamps @ 005ed3c0): count is
 * (size - 4) / 16, and it only logs each one, "Stamp %s(%d) at %I64d (start+%d, last+%d)", with the type's name
 * from StampLookup @ 017e6e88 and the timestamp relative to the first and to the previous stamp. Nothing else
 * reads them, so no type is required and the order is free. The type must be one of the values below (the name
 * table has exactly these 39 entries; a larger value reads past it).
 *
 * Live servers stamped real steps with unix timestamps in seconds: START, the LEGOINT_* steps (the LEGO account
 * web service, which checked the password), DB_INSERT_*, CLIENT_OS, then the WORLD_* / IM_* steps of handing the
 * session to the instance manager and picking a world server, ending with WORLD_COMMUNICATION_FINISH. The values
 * were step results (1 for done) or ids (such as the server the step talked to).
 *
 * What DLU stamps (AuthPackets::LoginStamps; every step adds its stamp as it happens):
 *   START (0)                       the login request arrived
 *   CLIENT_OS (the ClientOS)        the request was read
 *   DB_SELECT_START / _FINISH       looking up the account (FINISH value: 1 found, 0 not)
 *   LEGOINT_WEBSERVICE_START / _FINISH  checking the password (live asked the LEGO web service; DLU checks it
 *                                   itself; FINISH value: 1 matches, 0 not)
 *   ERROR (1)                       a check failed: unknown user, wrong password, play key, banned, locked
 *   GM_REQUIRED (1)                 the server is closed to non developers
 *   BYPASS (1)                      no play key needed (keys off, or a GM account)
 *   DB_INSERT_START / _FINISH       writing to the database (lifting an expired ban, recording the address)
 *   WORLD_DISCONNECT (1)            no master server to ask for a world
 *   WORLD_COMMUNICATION_START (0)   asked master for a world server
 *   WORLD_PACKET_RECEIVED (instance) master answered
 *   IM_COMMUNICATION_START / IM_LOGIN_START / IM_COMMUNICATION_END (1)  the session key was given to master
 *   WORLD_SESSION_CONFIRM_TO_AUTH (1), WORLD_COMMUNICATION_FINISH (world port)  the player is sent to the world
 * The auth server logs the same lines as the client (debug log) when it sends the response.
 */
enum class eStamps : uint32_t {
	PASSPORT_AUTH_START,
	PASSPORT_AUTH_BYPASS,
	PASSPORT_AUTH_ERROR,
	PASSPORT_AUTH_DB_SELECT_START,
	PASSPORT_AUTH_DB_SELECT_FINISH,
	PASSPORT_AUTH_DB_INSERT_START,
	PASSPORT_AUTH_DB_INSERT_FINISH,
	PASSPORT_AUTH_LEGOINT_COMMUNICATION_START,
	PASSPORT_AUTH_LEGOINT_RECEIVED,
	PASSPORT_AUTH_LEGOINT_THREAD_SPAWN,
	PASSPORT_AUTH_LEGOINT_WEBSERVICE_START,
	PASSPORT_AUTH_LEGOINT_WEBSERVICE_FINISH,
	PASSPORT_AUTH_LEGOINT_LEGOCLUB_START,
	PASSPORT_AUTH_LEGOINT_LEGOCLUB_FINISH,
	PASSPORT_AUTH_LEGOINT_THREAD_FINISH,
	PASSPORT_AUTH_LEGOINT_REPLY,
	PASSPORT_AUTH_LEGOINT_ERROR,
	PASSPORT_AUTH_LEGOINT_COMMUNICATION_END,
	PASSPORT_AUTH_LEGOINT_DISCONNECT,
	PASSPORT_AUTH_WORLD_COMMUNICATION_START,
	PASSPORT_AUTH_CLIENT_OS,
	PASSPORT_AUTH_WORLD_PACKET_RECEIVED,
	PASSPORT_AUTH_IM_COMMUNICATION_START,
	PASSPORT_AUTH_IM_LOGIN_START,
	PASSPORT_AUTH_IM_LOGIN_ALREADY_LOGGED_IN,
	PASSPORT_AUTH_IM_OTHER_LOGIN_REMOVED,
	PASSPORT_AUTH_IM_LOGIN_QUEUED,
	PASSPORT_AUTH_IM_LOGIN_RESPONSE,
	PASSPORT_AUTH_IM_COMMUNICATION_END,
	PASSPORT_AUTH_WORLD_SESSION_CONFIRM_TO_AUTH,
	PASSPORT_AUTH_WORLD_COMMUNICATION_FINISH,
	PASSPORT_AUTH_WORLD_DISCONNECT,
	NO_LEGO_INTERFACE,
	DB_ERROR,
	GM_REQUIRED,
	NO_LEGO_WEBSERVICE_XML,
	LEGO_WEBSERVICE_TIMEOUT,
	LEGO_WEBSERVICE_ERROR,
	NO_WORLD_SERVER
};

struct Stamp {
	eStamps type{};
	uint32_t value{};
	uint64_t timestamp{};

	Stamp() = default;
	Stamp(eStamps type, uint32_t value, uint64_t timestamp = time(nullptr)){
		this->type = type;
		this->value = value;
		this->timestamp = timestamp;
	}

	void Serialize(RakNet::BitStream& outBitStream) const;
	bool Deserialize(RakNet::BitStream& inBitStream);
};

// The stamps of one login. Each step adds its stamp when it happens, on whichever server performs it: the list
// travels with the login from auth to master and back inside the server messages (REQUEST_ZONE_TRANSFER and its
// response), and the login response finally carries it to the client.
// Written as a u32 holding 16 * count + 4, then the stamps (the login response's layout).
struct Stamps {
	std::vector<Stamp> list{};

	Stamps() = default;
	explicit Stamps(std::vector<Stamp> stamps) : list(std::move(stamps)) {}

	// Stamps a step that just happened, with the current time
	void Add(eStamps type, uint32_t value = 0);
	bool empty() const { return list.empty(); }
	size_t size() const { return list.size(); }

	void Serialize(RakNet::BitStream& bitStream) const;
	bool Deserialize(RakNet::BitStream& bitStream);

	// Logs each stamp (debug log) the way the client does
	void Log(const std::string& context) const;
};

#endif // STAMPS_H
