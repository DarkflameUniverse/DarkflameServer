#pragma once

#ifndef __DCOMMONVARS__H__
#define __DCOMMONVARS__H__

#include <cstdint>
#include <string>
#include <set>
#include "BitStream.h"
#include "eConnectionType.h"
#include "eClientMessageType.h"
#include "BitStreamUtils.h"

#pragma warning (disable:4251) //Disables SQL warnings

// These are the same define, but they mean two different things in different contexts
// so a different define to distinguish what calculation is happening will help clarity.
#define FRAMES_TO_MS(x) 1000 / x
#define MS_TO_FRAMES(x) 1000 / x

//=========== FRAME TIMINGS ===========
constexpr uint32_t highFramerate = 60;
constexpr uint32_t mediumFramerate = 30;
constexpr uint32_t lowFramerate = 15;

constexpr uint32_t highFrameDelta = FRAMES_TO_MS(highFramerate);
constexpr uint32_t mediumFrameDelta = FRAMES_TO_MS(mediumFramerate);
constexpr uint32_t lowFrameDelta = FRAMES_TO_MS(lowFramerate);

//========== MACROS ===========

#define HEADER_SIZE 8
#define CBITSTREAM RakNet::BitStream bitStream;
#define CINSTREAM RakNet::BitStream inStream(packet->data, packet->length, false);
#define CINSTREAM_SKIP_HEADER CINSTREAM if (inStream.GetNumberOfUnreadBits() >= BYTES_TO_BITS(HEADER_SIZE)) inStream.IgnoreBytes(HEADER_SIZE); else inStream.IgnoreBits(inStream.GetNumberOfUnreadBits());
#define CMSGHEADER BitStreamUtils::WriteHeader(bitStream, eConnectionType::CLIENT, eClientMessageType::GAME_MSG);
#define SEND_PACKET Game::server->Send(&bitStream, sysAddr, false);
#define SEND_PACKET_BROADCAST Game::server->Send(&bitStream, UNASSIGNED_SYSTEM_ADDRESS, true);

//=========== TYPEDEFS ==========

typedef int32_t LOT;                        //!< A LOT
typedef int64_t LWOOBJID;                   //!< An object ID (should be unsigned actually but ok)
typedef int32_t TSkillID;                   //!< A skill ID
typedef uint32_t LWOCLONEID;                 //!< Used for Clone IDs
typedef uint16_t LWOMAPID;                   //!< Used for Map IDs
typedef uint16_t LWOINSTANCEID;              //!< Used for Instance IDs
typedef uint32_t PROPERTYCLONELIST;          //!< Used for Property Clone IDs
typedef uint32_t StripId;

const LWOOBJID LWOOBJID_EMPTY = 0;          //!< An empty object ID
const LOT LOT_NULL = -1;                    //!< A null LOT
const int32_t LOOTTYPE_NONE = 0;            //!< No loot type available
const float SECONDARY_PRIORITY = 1.0f;      //!< Secondary Priority
const uint32_t INVENTORY_MAX = 9999999;     //!< The Maximum Inventory Size
const uint32_t LWOCLONEID_INVALID = -1;     //!< Invalid LWOCLONEID
const uint16_t LWOINSTANCEID_INVALID = -1;  //!< Invalid LWOINSTANCEID
const uint16_t LWOMAPID_INVALID = -1;       //!< Invalid LWOMAPID
const uint64_t LWOZONEID_INVALID = 0;       //!< Invalid LWOZONEID

const float PI = 3.14159f;

//============ STRUCTS ==============

struct LWOSCENEID {
public:
	LWOSCENEID() { m_sceneID = -1; m_layerID = 0; }
	LWOSCENEID(int sceneID) { m_sceneID = sceneID; m_layerID = 0; }
	LWOSCENEID(int sceneID, unsigned int layerID) { m_sceneID = sceneID; m_layerID = layerID; }

	LWOSCENEID& operator=(const LWOSCENEID& rhs) { m_sceneID = rhs.m_sceneID; m_layerID = rhs.m_layerID; return *this; }
	LWOSCENEID& operator=(const int rhs) { m_sceneID = rhs; m_layerID = 0; return *this; }

	bool operator<(const LWOSCENEID& rhs) const { return (m_sceneID < rhs.m_sceneID || (m_sceneID == rhs.m_sceneID && m_layerID < rhs.m_layerID)); }
	bool operator<(const int rhs) const { return m_sceneID < rhs; }

	bool operator==(const LWOSCENEID& rhs) const { return (m_sceneID == rhs.m_sceneID && m_layerID == rhs.m_layerID); }
	bool operator==(const int rhs) const { return m_sceneID == rhs; }

	const int GetSceneID() const { return m_sceneID; }
	const unsigned int GetLayerID() const { return m_layerID; }

	void SetSceneID(const int sceneID) { m_sceneID = sceneID; }
	void SetLayerID(const unsigned int layerID) { m_layerID = layerID; }

private:
	int m_sceneID;
	unsigned int m_layerID;
};

struct LWOZONEID {
public:
	const LWOMAPID& GetMapID() const { return m_MapID; }
	const LWOINSTANCEID& GetInstanceID() const { return m_InstanceID; }
	const LWOCLONEID& GetCloneID() const { return m_CloneID; }

	//In order: def constr, constr, assign op
	LWOZONEID() { m_MapID = LWOMAPID_INVALID; m_InstanceID = LWOINSTANCEID_INVALID; m_CloneID = LWOCLONEID_INVALID; }
	LWOZONEID(const LWOMAPID& mapID, const LWOINSTANCEID& instanceID, const LWOCLONEID& cloneID) { m_MapID = mapID; m_InstanceID = instanceID; m_CloneID = cloneID; }
	LWOZONEID(const LWOZONEID& replacement) { *this = replacement; }

private:
	LWOMAPID m_MapID; //1000 for VE, 1100 for AG, etc...
	LWOINSTANCEID m_InstanceID; //Instances host the same world, but on a different dWorld process.
	LWOCLONEID m_CloneID; //To differentiate between "your property" and "my property". Always 0 for non-prop worlds.
};

const LWOSCENEID LWOSCENEID_INVALID = -1;

struct LWONameValue {
	uint32_t length = 0;        //!< The length of the name
	std::u16string name;			//!< The name

	LWONameValue(void) {}

	LWONameValue(const std::u16string& name) {
		this->name = name;
		this->length = static_cast<uint32_t>(name.length());
	}

	~LWONameValue(void) {}
};

struct FriendData {
public:
	bool isOnline = false;
	bool isBestFriend = false;
	bool isFTP = false;
	LWOZONEID zoneID;
	LWOOBJID friendID;
	std::string friendName;

	void Serialize(RakNet::BitStream& bitStream) {
		bitStream.Write<uint8_t>(isOnline);
		bitStream.Write<uint8_t>(isBestFriend);
		bitStream.Write<uint8_t>(isFTP);
		bitStream.Write<uint32_t>(0); //???
		bitStream.Write<uint8_t>(0);  //???
		bitStream.Write(zoneID.GetMapID());
		bitStream.Write(zoneID.GetInstanceID());
		bitStream.Write(zoneID.GetCloneID());
		bitStream.Write(friendID);

		uint32_t maxSize = 33;
		uint32_t size = static_cast<uint32_t>(friendName.length());
		uint32_t remSize = static_cast<uint32_t>(maxSize - size);

		if (size > maxSize) size = maxSize;

		for (uint32_t i = 0; i < size; ++i) {
			bitStream.Write(static_cast<uint16_t>(friendName[i]));
		}

		for (uint32_t j = 0; j < remSize; ++j) {
			bitStream.Write(static_cast<uint16_t>(0));
		}

		bitStream.Write<uint32_t>(0); //???
		bitStream.Write<uint16_t>(0); //???
	}
};
#endif  //!__DCOMMONVARS__H__
