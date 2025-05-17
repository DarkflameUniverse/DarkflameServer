#pragma once

#ifndef __DTYPEDEFS__H__
#define __DTYPEDEFS__H__

#include <cstdint>

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

#endif // __DTYPEDEFS__H__
