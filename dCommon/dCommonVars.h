#pragma once

#include <cstdint>
#include <string>
#include <set>
#include "../thirdparty/raknet/Source/BitStream.h"

#pragma warning (disable:4251) //Disables SQL warnings

typedef int RESTICKET;

const int highFrameRate = 16;	//60fps
const int mediumFramerate = 33;	//30fps
const int lowFramerate = 66;	//15fps

//========== MACROS ===========

#define CBITSTREAM RakNet::BitStream bitStream;
#define CINSTREAM RakNet::BitStream inStream(packet->data, packet->length, false);
#define CMSGHEADER PacketUtils::WriteHeader(bitStream, CLIENT, MSG_CLIENT_GAME_MSG);
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

typedef int32_t PetTamingPiece;             //!< Pet Taming Pieces

const LWOOBJID LWOOBJID_EMPTY = 0;          //!< An empty object ID
const LOT LOT_NULL = -1;                    //!< A null LOT
const int32_t LOOTTYPE_NONE = 0;            //!< No loot type available
const float SECONDARY_PRIORITY = 1.0f;      //!< Secondary Priority
const uint32_t INVENTORY_INVALID = -1;      //!< Invalid Inventory
const uint32_t INVENTORY_DEFAULT = -1;      //!< Default Inventory
const uint32_t StatusChangeInfo = 0;        //!< Status Change Info (???)
const uint32_t INVENTORY_MAX = 9999999;     //!< The Maximum Inventory Size
const uint32_t LWOCLONEID_INVALID = -1;     //!< Invalid LWOCLONEID
const uint16_t LWOINSTANCEID_INVALID = -1;  //!< Invalid LWOINSTANCEID
const uint16_t LWOMAPID_INVALID = -1;       //!< Invalid LWOMAPID
const uint64_t LWOZONEID_INVALID = 0;       //!< Invalid LWOZONEID

typedef std::set<LWOOBJID> TSetObjID;

const float PI = 3.14159f;

#if defined(__unix) || defined(__APPLE__)
//For Linux:
typedef __int64_t __int64;
#endif

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

struct Brick {
    uint32_t designerID;
    uint32_t materialID;
};

//This union is used by the behavior system
union suchar {
	unsigned char usigned;
	char svalue;
};

//=========== DLU ENUMS ============

enum eGameMasterLevel : int32_t {
    GAME_MASTER_LEVEL_CIVILIAN = 0,             // Normal player.
    GAME_MASTER_LEVEL_FORUM_MODERATOR = 1,      // No permissions on live servers.
    GAME_MASTER_LEVEL_JUNIOR_MODERATOR = 2,     // Can kick/mute and pull chat logs.
    GAME_MASTER_LEVEL_MODERATOR = 3,            // Can return lost items.
    GAME_MASTER_LEVEL_SENIOR_MODERATOR = 4,     // Can ban.
    GAME_MASTER_LEVEL_LEAD_MODERATOR = 5,       // Can approve properties.
    GAME_MASTER_LEVEL_JUNIOR_DEVELOPER = 6,     // Junior developer & future content team. Civilan on live.
    GAME_MASTER_LEVEL_INACTIVE_DEVELOPER = 7,   // Inactive developer, limited permissions.
    GAME_MASTER_LEVEL_DEVELOPER = 8,            // Active developer, full permissions on live.
    GAME_MASTER_LEVEL_OPERATOR = 9              // Can shutdown server for restarts & updates.
};

//=========== LU ENUMS ============

//! An enum for object ID bits
enum eObjectBits : int32_t {
    OBJECT_BIT_PERSISTENT = 32,     //!< The 32 bit index
    OBJECT_BIT_CLIENT = 46,         //!< The 46 bit index
    OBJECT_BIT_SPAWNED = 58,        //!< The 58 bit index
    OBJECT_BIT_CHARACTER = 60       //!< The 60 bit index
};

//! An enum for MatchUpdate types
enum eMatchUpdate : int {
	MATCH_UPDATE_PLAYER_JOINED = 0,
	MATCH_UPDATE_PLAYER_LEFT = 1,
	MATCH_UPDATE_TIME = 3,
	MATCH_UPDATE_TIME_START_DELAY = 4,
	MATCH_UPDATE_PLAYER_READY = 5,
	MATCH_UPDATE_PLAYER_UNREADY = 6
};

//! An enum for camera cycling modes
enum eCyclingMode : uint32_t {
	ALLOW_CYCLE_TEAMMATES,
	DISALLOW_CYCLING
};

enum eCinematicEvent : uint32_t {
    STARTED,
    WAYPOINT,
    ENDED,
};

//! An enum for character creation responses
enum eCreationResponse : uint8_t {
    CREATION_RESPONSE_SUCCESS = 0,                  //!< The creation was successful
    CREATION_RESPONSE_OBJECT_ID_UNAVAILABLE,        //!< The Object ID can't be used
    CREATION_RESPONSE_NAME_NOT_ALLOWED,             //!< The name is not allowed
    CREATION_RESPONSE_PREDEFINED_NAME_IN_USE,       //!< The predefined name is already in use
    CREATION_RESPONSE_CUSTOM_NAME_IN_USE            //!< The custom name is already in use
};

//! An enum for login responses
enum eLoginResponse : uint8_t {
	LOGIN_RESPONSE_GENERAL_FAILED = 0,
	LOGIN_RESPONSE_SUCCESS = 1,
	LOGIN_RESPONSE_BANNED = 2,
	LOGIN_RESPONSE_PERMISSIONS_NOT_HIGH_ENOUGH = 5,
	LOGIN_RESPONSE_WRONG_PASS_OR_USER = 6,
	LOGIN_RESPONSE_ACCOUNT_LOCKED = 7
};

//! An enum for character rename responses
enum eRenameResponse : uint8_t {
    RENAME_RESPONSE_SUCCESS = 0,                    //!< The renaming was successful
    RENAME_RESPONSE_UNKNOWN_ERROR,                  //!< There was an unknown error
    RENAME_RESPONSE_NAME_UNAVAILABLE,               //!< The name is unavailable
    RENAME_RESPONSE_NAME_IN_USE                     //!< The name is already in use
};

//! A replica packet type
enum eReplicaPacketType {
    PACKET_TYPE_CONSTRUCTION,                       //!< A construction packet
    PACKET_TYPE_SERIALIZATION,                      //!< A serialization packet
    PACKET_TYPE_DESTRUCTION                         //!< A destruction packet
};

enum ServerDisconnectIdentifiers {
    SERVER_DISCON_UNKNOWN_SERVER_ERROR = 0,               //!< Unknown server error
	SERVER_DISCON_DUPLICATE_LOGIN = 4,                    //!< Used when another user with the same username is logged in (duplicate login)
	SERVER_DISCON_SERVER_SHUTDOWN = 5,                    //!< Used when the server is shutdown
	SERVER_DISCON_SERVER_MAP_LOAD_FAILURE = 6,            //!< Used when the server cannot load a map
	SERVER_DISCON_INVALID_SESSION_KEY = 7,                //!< Used if the session is invalid
	SERVER_DISCON_ACCOUNT_NOT_IN_PENDING_LIST = 8,        //!< ???
	SERVER_DISCON_CHARACTER_NOT_FOUND = 9,                //!< Used if a character that the server has is not found (i.e, corruption with user-player data)
	SERVER_DISCON_CHARACTER_CORRUPTED = 10,               //!< Similar to abovce
	SERVER_DISCON_KICK = 11,                              //!< Used if the user is kicked from the server
	SERVER_DISCON_FREE_TRIAL_EXPIRED = 12,                //!< Used if the user's free trial expired
	SERVER_DISCON_PLAY_SCHEDULE_TIME_DONE = 13            //!< Used if the user's play time is used up
};

//! The Behavior Types for use with the AI system
enum eCombatBehaviorTypes : uint32_t {
    PASSIVE = 0,                    //!< The object is passive
    AGGRESSIVE = 1,                 //!< The object is aggressive
    PASSIVE_TURRET = 2,             //!< The object is a passive turret
    AGGRESSIVE_TURRET = 3           //!< The object is an aggressive turret
};

//! The Combat Role Type for use with the AI system
enum eCombatRoleType : uint32_t {
    MELEE = 0,                      //!< Used for melee attacks
    RANGED = 1,                     //!< Used for range attacks
    SUPPORT = 2                     //!< Used for support
};

//! The kill types for the Die packet
enum eKillType : uint32_t {
    VIOLENT,
    SILENT
};

//! The various world states used throughout the server
enum eObjectWorldState {
    WORLDSTATE_INWORLD,             //!< Probably used when the object is in the world
    WORLDSTATE_ATTACHED,            //!< Probably used when the object is attached to another object
    WORLDSTATE_INVENTORY            //!< Probably used when the object is in an inventory
};

//! The trigger stats (???)
enum eTriggerStat {
    INVALID_STAT,                   //!< ???
    HEALTH,                         //!< Probably used for health
    ARMOR,                          //!< Probably used for armor
    IMAGINATION                     //!< Probably used for imagination
};

//! The trigger operations (???)
enum eTriggerOperator {
    INVALID_OPER,                   //!< ???
    EQUAL,                          //!< ???
    NOT_EQUAL,                      //!< ???
    GREATER,                        //!< ???
    GREATER_EQUAL,                  //!< ???
    LESS,                           //!< ???
    LESS_EQUAL                      //!< ???
};

//! The various build types
enum eBuildType {
    BUILD_NOWHERE,                  //!< Used if something can't be built anywhere
    BUILD_IN_WORLD,                 //!< Used if something can be built in the world
    BUILD_ON_PROPERTY               //!< Used if something can be build on a property
};

//! Quickbuild fail reasons
enum eFailReason : uint32_t {
    REASON_NOT_GIVEN,
    REASON_OUT_OF_IMAGINATION,
    REASON_CANCELED_EARLY,
    REASON_BUILD_ENDED
};

//! Terminate interaction type
enum eTerminateType : uint32_t {
    RANGE,
    USER,
    FROM_INTERACTION
};

//! The combat state
enum eCombatState {
    IDLE,                           //!< The AI is in an idle state
    AGGRO,                          //!< The AI is in an aggressive state
    TETHER,                         //!< The AI is being redrawn back to tether point
    SPAWN,                          //!< The AI is spawning
    DEAD                            //!< The AI is dead
};

enum eControlSceme {
	SCHEME_A,
	SCHEME_D,
	SCHEME_GAMEPAD,
	SCHEME_E,
	SCHEME_FPS,
	SCHEME_DRIVING,
	SCHEME_TAMING,
	SCHEME_MODULAR_BUILD,
	SCHEME_WEAR_A_ROBOT //== freecam? 
};

enum eStunState {
	PUSH,
	POP
};

enum eNotifyType {
    NOTIFY_TYPE_SUCCESS,
    NOTIFY_TYPE_QUIT,
    NOTIFY_TYPE_FAILED,
    NOTIFY_TYPE_BEGIN,
    NOTIFY_TYPE_READY,
    NOTIFY_TYPE_NAMINGPET
};

enum eReplicaComponentType : int32_t {
    COMPONENT_TYPE_CONTROLLABLE_PHYSICS     = 1,            //!< The ControllablePhysics Component
    COMPONENT_TYPE_RENDER                   = 2,            //!< The Render Component
    COMPONENT_TYPE_SIMPLE_PHYSICS           = 3,            //!< The SimplePhysics Component
    COMPONENT_TYPE_CHARACTER                = 4,            //!< The Character Component
    COMPONENT_TYPE_SCRIPT                   = 5,            //!< The Script Component
    COMPONENT_TYPE_BOUNCER                  = 6,            //!< The Bouncer Component
    COMPONENT_TYPE_BUFF                     = 7,            //!< The Buff Component
    COMPONENT_TYPE_SKILL                    = 9,            //!< The Skill Component
	COMPONENT_TYPE_ITEM                     = 11,           //!< The Item Component
    COMPONENT_TYPE_VENDOR                   = 16,           //!< The Vendor Component
    COMPONENT_TYPE_INVENTORY                = 17,           //!< The Inventory Component
    COMPONENT_TYPE_SHOOTING_GALLERY         = 19,           //!< The Shooting Gallery Component
    COMPONENT_TYPE_RIGID_BODY_PHANTOM_PHYSICS   = 20,       //!< The RigidBodyPhantomPhysics Component
    COMPONENT_TYPE_COLLECTIBLE              = 23,           //!< The Collectible Component
    COMPONENT_TYPE_MOVING_PLATFORM          = 25,           //!< The MovingPlatform Component
    COMPONENT_TYPE_PET                      = 26,           //!< The Pet Component
    COMPONENT_TYPE_VEHICLE_PHYSICS          = 30,           //!< The VehiclePhysics Component
	COMPONENT_TYPE_MOVEMENT_AI              = 31,           //!< The MovementAI Component
	COMPONENT_TYPE_PROPERTY                 = 36,           //!< The Property Component
    COMPONENT_TYPE_SCRIPTED_ACTIVITY        = 39,           //!< The ScriptedActivity Component
    COMPONENT_TYPE_PHANTOM_PHYSICS          = 40,           //!< The PhantomPhysics Component
	COMPONENT_TYPE_MODEL 					= 42,			//!< The Model Component
    COMPONENT_TYPE_PROPERTY_ENTRANCE        = 43,           //!< The PhantomPhysics Component
	COMPONENT_TYPE_PROPERTY_MANAGEMENT      = 45,           //!< The PropertyManagement Component
    COMPONENT_TYPE_REBUILD                  = 48,           //!< The Rebuild Component
    COMPONENT_TYPE_SWITCH                   = 49,           //!< The Switch Component
    COMPONENT_TYPE_ZONE_CONTROL             = 50,           //!< The ZoneControl Component
	COMPONENT_TYPE_PACKAGE                  = 53,           //!< The Package Component
    COMPONENT_TYPE_PLAYER_FLAG              = 58,           //!< The PlayerFlag Component
    COMPONENT_TYPE_BASE_COMBAT_AI           = 60,           //!< The BaseCombatAI Component
    COMPONENT_TYPE_MODULE_ASSEMBLY          = 61,           //!< The ModuleAssembly Component
	COMPONENT_TYPE_PROPERTY_VENDOR          = 65,           //!< The PropertyVendor Component
    COMPONENT_TYPE_ROCKET_LAUNCH            = 67,           //!< The RocketLaunch Component
    COMPONENT_TYPE_RACING_CONTROL           = 71,           //!< The RacingControl Component
    COMPONENT_TYPE_MISSION_OFFER            = 73,           //!< The MissionOffer Component
    COMPONENT_TYPE_EXHIBIT                  = 75,           //!< The Exhibit Component
    COMPONENT_TYPE_RACING_STATS             = 74,           //!< The Exhibit Component
    COMPONENT_TYPE_SOUND_TRIGGER            = 77,           //!< The Sound Trigger Component
	COMPONENT_TYPE_PROXIMITY_MONITOR        = 78,           //!< The Proximity Monitor Component
    COMPONENT_TYPE_MISSION                  = 84,           //!< The Mission Component
    COMPONENT_TYPE_ROCKET_LAUNCH_LUP        = 97,           //!< The LUP Launchpad Componen
    COMPONENT_TYPE_RAIL_ACTIVATOR           = 104,
	COMPONENT_TYPE_POSSESSABLE				= 108,			//!< The Possessable Component
	COMPONENT_TYPE_POSSESSOR				= 110,			//!< The Possessor Component
	COMPONENT_TYPE_BUILD_BORDER             = 114,          //!< The Build Border Component
    COMPONENT_TYPE_DESTROYABLE              = 1000,         //!< The Destroyable Component
};

enum class UseItemResponse : uint32_t {
    NoImaginationForPet = 1,
    FailedPrecondition,
    MountsNotAllowed
};

/**
 * Represents the different types of inventories an entity may have
 */
enum eInventoryType : uint32_t {
    ITEMS = 0,
    VAULT_ITEMS,
    BRICKS,
    TEMP_ITEMS = 4,
    MODELS,
    TEMP_MODELS,
    BEHAVIORS,
    PROPERTY_DEEDS,
	VENDOR_BUYBACK = 11,
    HIDDEN = 12, //Used for missional items
    VAULT_MODELS = 14,
	ITEM_SETS, //internal
	INVALID // made up, for internal use!!!
};

enum eRebuildState : uint32_t {
	REBUILD_OPEN,
	REBUILD_COMPLETED = 2,
	REBUILD_RESETTING = 4,
	REBUILD_BUILDING,
	REBUILD_INCOMPLETE
};

/**
 * The loot source's type.  
 */
enum eLootSourceType : int32_t {
	LOOT_SOURCE_NONE = 0,
	LOOT_SOURCE_CHEST,
	LOOT_SOURCE_MISSION,
	LOOT_SOURCE_MAIL,
	LOOT_SOURCE_CURRENCY,
	LOOT_SOURCE_ACHIEVEMENT,
	LOOT_SOURCE_TRADE,
	LOOT_SOURCE_QUICKBUILD,
	LOOT_SOURCE_DELETION,
	LOOT_SOURCE_VENDOR,
	LOOT_SOURCE_ACTIVITY,
	LOOT_SOURCE_PICKUP,
	LOOT_SOURCE_BRICK,
	LOOT_SOURCE_PROPERTY,
	LOOT_SOURCE_MODERATION,
	LOOT_SOURCE_EXHIBIT,
	LOOT_SOURCE_INVENTORY,
	LOOT_SOURCE_CLAIMCODE,
	LOOT_SOURCE_CONSUMPTION,
	LOOT_SOURCE_CRAFTING,
	LOOT_SOURCE_LEVEL_REWARD,
	LOOT_SOURCE_RELOCATE
};

enum eGameActivities : uint32_t {
	ACTIVITY_NONE,
	ACTIVITY_QUICKBUILDING,
	ACTIVITY_SHOOTING_GALLERY,
	ACTIVITY_RACING,
	ACTIVITY_PINBALL,
	ACTIVITY_PET_TAMING
};

enum ePlayerFlags {
	BTARR_TESTING = 0,
	PLAYER_HAS_ENTERED_PET_RANCH = 1,
	MINIMAP_UNLOCKED = 2,
	ACTIVITY_REBUILDING_FAIL_TIME = 3,
	ACTIVITY_REBUILDING_FAIL_RANGE = 4,
	ACTIVITY_SHOOTING_GALLERY_HELP = 5,
	HELP_WALKING_CONTROLS = 6,
	FIRST_SMASHABLE = 7,
	FIRST_IMAGINATION_PICKUP = 8,
	FIRST_DAMAGE = 9,
	FIRST_ITEM = 10,
	FIRST_BRICK = 11,
	FIRST_CONSUMABLE = 12,
	FIRST_EQUIPPABLE = 13,
	CHAT_HELP = 14,
	FIRST_PET_TAMING_MINIGAME = 15,
	FIRST_PET_ON_SWITCH = 16,
	FIRST_PET_JUMPED_ON_SWITCH = 17,
	FIRST_PET_FOUND_TREASURE = 18,
	FIRST_PET_DUG_TREASURE = 19,
	FIRST_PET_OWNER_ON_PET_BOUNCER = 20,
	FIRST_PET_DESPAWN_NO_IMAGINATION = 21,
	FIRST_PET_SELECTED_ENOUGH_BRICKS = 22,
	FIRST_EMOTE_UNLOCKED = 23,
	GF_PIRATE_REP = 24,
	AG_BOB_CINEMATIC_EVENT = 25,
	HELP_JUMPING_CONTROLS = 26,
	HELP_DOUBLE_JUMP_CONTROLS = 27,
	HELP_CAMERA_CONTROLS = 28,
	HELP_ROTATE_CONTROLS = 29,
	HELP_SMASH = 30,
	MONUMENT_INTRO_MUSIC_PLAYED = 31,
	BEGINNING_ZONE_SUMMARY_DISPLAYED = 32,
	AG_FINISH_LINE_BUILT = 33,
	AG_BOSS_AREA_FOUND = 34,
	AG_LANDED_IN_BATTLEFIELD = 35,
	GF_PLAYER_HAS_BEEN_TO_THE_RAVINE = 36,
	MODULAR_BUILD_STARTED = 37,
	MODULAR_BUILD_FINISHED_CLICK_BUTTON = 38,
	THINKING_HAT_RECEIVED_GO_TO_MODULAR_BUILD_AREA = 39,
	BUILD_AREA_ENTERED_MOD_NOT_ACTIVATED_PUT_ON_HAT = 40,
	HAT_ON_INSIDE_OF_MOD_BUILD_EQUIP_A_MODULE_FROM_LEG = 41,
	MODULE_EQUIPPED_PLACE_ON_GLOWING_BLUE_SPOT = 42,
	FIRST_MODULE_PLACED_CORRECTLY_NOW_DO_THE_REST = 43,
	ROCKET_COMPLETE_NOW_LAUNCH_FROM_PAD = 44,
	JOINED_A_FACTION = 45,
	VENTURE_FACTION = 46,
	ASSEMBLY_FACTION = 47,
	PARADOX_FACTION = 48,
	SENTINEL_FACTION = 49,
	LUP_WORLD_ACCESS = 50,
	AG_FIRST_FLAG_COLLECTED = 51,
	TOOLTIP_TALK_TO_SKYLAND_TO_GET_HAT = 52,
	MODULAR_BUILD_PLAYER_PLACES_FIRST_MODEL_IN_SCRATCH = 53,
	MODULAR_BUILD_FIRST_ARROW_DISPLAY_FOR_MODULE = 54,
	AG_BEACON_QB,_SO_THE_PLAYER_CAN_ALWAYS_BUILD_THEM = 55,
	GF_PET_DIG_FLAG_1 = 56,
	GF_PET_DIG_FLAG_2 = 57,
	GF_PET_DIG_FLAG_3 = 58,
	SUPPRESS_SPACESHIP_CINEMATIC_FLYTHROUGH = 59,
	GF_PLAYER_FALL_DEATH = 60,
	GF_PLAYER_CAN_GET_FLAG_1 = 61,
	GF_PLAYER_CAN_GET_FLAG_2 = 62,
	GF_PLAYER_CAN_GET_FLAG_3 = 63,
	ENTER_BBB_FROM_PROPERTY_EDIT_CONFIRMATION_DIALOG = 64,
	AG_FIRST_COMBAT_COMPLETE = 65,
	AG_COMPLETE_BOB_MISSION = 66,
    NJ_GARMADON_CINEMATIC_SEEN = 125,
	ELEPHANT_PET_3050 = 801,
	CAT_PET_3054 = 802,
	TRICERATOPS_PET_3195 = 803,
	TERRIER_PET_3254 = 804,
	SKUNK_PET_3261 = 805,
	LION_PET_3520 = 806,
	BUNNY_PET_3672 = 807,
	CROCODILE_PET_3994 = 808,
	DOBERMAN_PET_5635 = 809,
	BUFFALO_PET_5636 = 810,
	ROBOT_DOG_PET_5637 = 811,
	EUROPEAN_DRAGON_PET_5639 = 812,
	TORTOISE_PET_5640 = 813,
	ASIAN_DRAGON_PET_5641 = 814,
	MANTIS_PET_5642 = 815,
	PANDA_PET_5643 = 816,
	WARTHOG_PET_6720 = 817,
	GOAT_PET_7638 = 818,
	CRAB_PET_7694 = 819,
	AG_SPACE_SHIP_BINOC_AT_LAUNCH = 1001,
	AG_SPACE_SHIP_BINOC_AT_LAUNCH_PLATFORM = 1002,
	AG_SPACE_SHIP_BINOC_ON_PLATFORM_TO_LEFT_OF_START = 1003,
	AG_SPACE_SHIP_BINOC_ON_PLATFORM_TO_RIGHT_OF_START = 1004,
	AG_SPACE_SHIP_BINOC_AT_BOB = 1005,
	AG_BATTLE_BINOC_FOR_TRICERETOPS = 1101,
	AG_BATTLE_BINOC_AT_PARADOX = 1102,
	AG_BATTLE_BINOC_AT_MISSION_GIVER = 1103,
	AG_BATTLE_BINOC_AT_BECK = 1104,
	AG_MONUMENT_BINOC_INTRO = 1105,
	AG_MONUMENT_BINOC_OUTRO = 1106,
	AG_LAUNCH_BINOC_INTRO = 1107,
	AG_LAUNCH_BINOC_BISON = 1108,
	AG_LAUNCH_BINOC_SHARK = 1109,
	NS_BINOC_CONCERT_TRANSITION = 1201,
	NS_BINOC_RACE_PLACE_TRANSITION = 1202,
	NS_BINOC_BRICK_ANNEX_TRANSITION = 1203,
	NS_BINOC_GF_LAUNCH = 1204,
	NS_BINOC_FV_LAUNCH = 1205,
	NS_BINOC_BRICK_ANNEX_WATER = 1206,
	NS_BINOC_AG_LAUNCH_AT_RACE_PLACE = 1207,
	NS_BINOC_AG_LAUNCH_AT_BRICK_ANNEX = 1208,
	NS_BINOC_AG_LAUNCH_AT_PLAZA = 1209,
	NS_BINOC_TBA = 1210,
	NS_FLAG_COLLECTABLE_1_BY_JONNY_THUNDER = 1211,
	NS_FLAG_COLLECTABLE_2_UNDER_CONCERT_BRIDGE = 1212,
	NS_FLAG_COLLECTABLE_3_BY_FV_LAUNCH = 1213,
	NS_FLAG_COLLECTABLE_4_IN_PLAZA_BEHIND_BUILDING = 1214,
	NS_FLAG_COLLECTABLE_5_BY_GF_LAUNCH = 1215,
	NS_FLAG_COLLECTABLE_6_BY_DUCK_SG = 1216,
	NS_FLAG_COLLECTABLE_7_BY_LUP_LAUNCH = 1217,
	NS_FLAG_COLLECTABLE_8_BY_NT_LUANCH = 1218,
	NS_FLAG_COLLECTABLE_9_BY_RACE_BUILD = 1219,
	NS_FLAG_COLLECTABLE_10_ON_AG_LAUNCH_PATH = 1220,
	PR_BINOC_AT_LAUNCH_PAD = 1251,
	PR_BINOC_AT_BEGINNING_OF_ISLAND_B = 1252,
	PR_BINOC_AT_FIRST_PET_BOUNCER = 1253,
	PR_BINOC_ON_BY_CROWS_NEST = 1254,
	PR_PET_DIG_AT_BEGINNING_OF_ISLAND_B = 1261,
	PR_PET_DIG_AT_THE_LOCATION_OF_OLD_BOUNCE_BACK = 1262,
	PR_PET_DIG_UNDER_QB_BRIDGE = 1263,
	PR_PET_DIG_BACK_SIDE_BY_PARTNER_BOUNCE = 1264,
	PR_PET_DIG_BY_LAUNCH_PAD = 1265,
	PR_PET_DIG_BY_FIRST_PET_BOUNCER = 1266,
	GF_BINOC_ON_LANDING_PAD = 1301,
	GF_BINOC_AT_RAVINE_START = 1302,
	GF_BINOC_ON_TOP_OF_RAVINE_HEAD = 1303,
	GF_BINOC_AT_TURTLE_AREA = 1304,
	GF_BINOC_IN_TUNNEL_TO_ELEPHANTS = 1305,
	GF_BINOC_IN_ELEPHANTS_AREA = 1306,
	GF_BINOC_IN_RACING_AREA = 1307,
	GF_BINOC_IN_CROC_AREA = 1308,
	GF_BINOC_IN_JAIL_AREA = 1309,
	GF_BINOC_TELESCOPE_NEXT_TO_CAPTAIN_JACK = 1310,
	NT_FACTION_SPY_DUKE = 1974,
	NT_FACTION_SPY_OVERBUILD = 1976,
	NT_FACTION_SPY_HAEL = 1977,
    NJ_EARTH_SPINJITZU = 2030,
    NJ_LIGHTNING_SPINJITZU = 2031,
    NJ_ICE_SPINJITZU = 2032,
    NJ_FIRE_SPINJITZU = 2033,
    NJ_WU_SHOW_DAILY_CHEST = 2099
};

//======== FUNC ===========

template<typename T>
inline T const& clamp(const T& val, const T& low, const T& high) {
    if (val < low) return low;
    else if (val > high) return high;
    
    return val;
}
