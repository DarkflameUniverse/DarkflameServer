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

using LOT = int32_t;                        //!< A LOT
using LWOOBJID = int64_t;                   //!< An object ID (should be unsigned actually but ok)
using TSkillID = int32_t;                   //!< A skill ID
using LWOCLONEID = uint32_t;                 //!< Used for Clone IDs
using LWOMAPID = uint16_t;                   //!< Used for Map IDs
using LWOINSTANCEID = uint16_t;              //!< Used for Instance IDs
using PROPERTYCLONELIST = uint32_t;          //!< Used for Property Clone IDs
using StripId = uint32_t;

constexpr LWOOBJID LWOOBJID_EMPTY = 0;          	//!< An empty object ID
constexpr LOT LOT_NULL = -1;                    	//!< A null LOT
constexpr int32_t LOOTTYPE_NONE = 0;            	//!< No loot type available
constexpr float SECONDARY_PRIORITY = 1.0f;      	//!< Secondary Priority
constexpr uint32_t INVENTORY_MAX = 9999999;     	//!< The Maximum Inventory Size
constexpr LWOCLONEID LWOCLONEID_INVALID = -1;     	//!< Invalid LWOCLONEID
constexpr LWOINSTANCEID LWOINSTANCEID_INVALID = -1; //!< Invalid LWOINSTANCEID
constexpr LWOMAPID LWOMAPID_INVALID = -1;       	//!< Invalid LWOMAPID
constexpr uint64_t LWOZONEID_INVALID = 0;       	//!< Invalid LWOZONEID

constexpr float PI = 3.14159f;

//============ STRUCTS ==============

struct LWOSCENEID {
public:
	constexpr LWOSCENEID() noexcept { m_sceneID = -1; m_layerID = 0; }
	constexpr LWOSCENEID(int32_t sceneID) noexcept { m_sceneID = sceneID; m_layerID = 0; }
	constexpr LWOSCENEID(int32_t sceneID, uint32_t layerID) noexcept { m_sceneID = sceneID; m_layerID = layerID; }

	constexpr LWOSCENEID& operator=(const LWOSCENEID& rhs) noexcept { m_sceneID = rhs.m_sceneID; m_layerID = rhs.m_layerID; return *this; }
	constexpr LWOSCENEID& operator=(const int32_t rhs) noexcept { m_sceneID = rhs; m_layerID = 0; return *this; }

	constexpr bool operator<(const LWOSCENEID& rhs) const noexcept { return (m_sceneID < rhs.m_sceneID || (m_sceneID == rhs.m_sceneID && m_layerID < rhs.m_layerID)); }
	constexpr bool operator<(const int32_t rhs) const noexcept { return m_sceneID < rhs; }

	constexpr bool operator==(const LWOSCENEID& rhs) const noexcept { return (m_sceneID == rhs.m_sceneID && m_layerID == rhs.m_layerID); }
	constexpr bool operator==(const int32_t rhs) const noexcept { return m_sceneID == rhs; }

	constexpr int32_t GetSceneID() const noexcept { return m_sceneID; }
	constexpr uint32_t GetLayerID() const noexcept { return m_layerID; }

	constexpr void SetSceneID(const int32_t sceneID) noexcept { m_sceneID = sceneID; }
	constexpr void SetLayerID(const uint32_t layerID) noexcept { m_layerID = layerID; }

private:
	int32_t m_sceneID;
	uint32_t m_layerID;
};

struct LWOZONEID {
public:
	constexpr const LWOMAPID& GetMapID() const noexcept { return m_MapID; }
	constexpr const LWOINSTANCEID& GetInstanceID() const noexcept { return m_InstanceID; }
	constexpr const LWOCLONEID& GetCloneID() const noexcept { return m_CloneID; }

	//In order: def constr, constr, assign op
	constexpr LWOZONEID() noexcept = default;
	constexpr LWOZONEID(const LWOMAPID& mapID, const LWOINSTANCEID& instanceID, const LWOCLONEID& cloneID) noexcept { m_MapID = mapID; m_InstanceID = instanceID; m_CloneID = cloneID; }
	constexpr LWOZONEID(const LWOZONEID& replacement) noexcept { *this = replacement; }

private:
	LWOMAPID m_MapID = LWOMAPID_INVALID; //1000 for VE, 1100 for AG, etc...
	LWOINSTANCEID m_InstanceID = LWOINSTANCEID_INVALID; //Instances host the same world, but on a different dWorld process.
	LWOCLONEID m_CloneID = LWOCLONEID_INVALID; //To differentiate between "your property" and "my property". Always 0 for non-prop worlds.
};

constexpr LWOSCENEID LWOSCENEID_INVALID = -1;

struct LWONameValue {
	uint32_t length = 0;        //!< The length of the name
	std::u16string name;			//!< The name

	LWONameValue() = default;

	LWONameValue(const std::u16string& name) {
		this->name = name;
		this->length = static_cast<uint32_t>(name.length());
	}
};

struct FriendData {
public:
	bool isOnline = false;
	bool isBestFriend = false;
	bool isFTP = false;
	LWOZONEID zoneID;
	LWOOBJID friendID;
	std::string friendName;

	void Serialize(RakNet::BitStream& bitStream) const {
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
			bitStream.Write<uint16_t>(friendName[i]);
		}

		for (uint32_t j = 0; j < remSize; ++j) {
			bitStream.Write<uint16_t>(0);
		}

		bitStream.Write<uint32_t>(0); //???
		bitStream.Write<uint16_t>(0); //???
	}
};

#endif  //!__DCOMMONVARS__H__
