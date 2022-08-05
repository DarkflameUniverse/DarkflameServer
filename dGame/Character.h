#ifndef CHARACTER_H
#define CHARACTER_H

#include "dCommonVars.h"
#include <vector>
#include "../thirdparty/tinyxml2/tinyxml2.h"
#include <unordered_map>
#include <map>

#include "NiPoint3.h"
#include "NiQuaternion.h"
#include "PermissionMap.h"

class User;
struct Packet;
class Entity;

/**
 * Meta information about a character, like their name and style
 */
class Character {
public:
	Character(uint32_t id, User* parentUser);
	~Character();

	void SaveXMLToDatabase();
	void UpdateFromDatabase();

	void SaveXmlRespawnCheckpoints();
	void LoadXmlRespawnCheckpoints();

	const std::string& GetXMLData() const { return m_XMLData; }
	tinyxml2::XMLDocument* GetXMLDoc() const { return m_Doc; }

	/**
	 * Gets the database ID of the character
	 * @return the database ID of the character
	 */
	uint32_t GetID() const { return m_ID; }

	/**
	 * Gets the (custom) name of the character
	 * @return the name of the character
	 */
	const std::string& GetName() const { return m_Name; }

	/**
	 * Gets the generated name of the character
	 * @return the generated name
	 */
	const std::string& GetUnapprovedName() const { return m_UnapprovedName; }

	/**
	 * Gets whether or not the custom name for this character was rejected
	 * @return whether the custom name for this character was rejected
	 */
	bool GetNameRejected() const { return m_NameRejected; }

	/**
	 * Gets the object ID of the entity this character belongs to
	 * @return the object ID of the entity this character belongs to
	 */
	LWOOBJID GetObjectID() const { return m_ObjectID; }

	/**
	 * Gets the identifier for the properties of this character
	 * @return The identifier for the properties of this character
	 */
	uint32_t GetPropertyCloneID() const { return m_PropertyCloneID; }

	/**
	 * Gets the last login of this character in MS
	 * @return the last login of this character
	 */
	uint64_t GetLastLogin() const { return m_LastLogin; }


	/**
	 * Gets the default shirt color for this character
	 * @return the default shirt color ID
	 */
	uint32_t GetShirtColor() const { return m_ShirtColor; }

	/**
	 * Gets the default hair style for this character
	 * @return the default hair style ID
	 */
	uint32_t GetShirtStyle() const { return m_ShirtStyle; }

	/**
	 * Gets the default pants color for this character
	 * @return the default pants color ID
	 */
	uint32_t GetPantsColor() const { return m_PantsColor; }

	/**
	 * Gets the default hair color for this character
	 * @return the default hair color ID
	 */
	uint32_t GetHairColor() const { return m_HairColor; }

	/**
	 * Gets the default hair style of this character
	 * @return the default hair style ID
	 */
	uint32_t GetHairStyle() const { return m_HairStyle; }

	/**
	 * Gets the eyes config for this character
	 * @return the eyes config ID
	 */
	uint32_t GetEyes() const { return m_Eyes; }

	/**
	 * Gets the eyebrows config for this character
	 * @return the eyebrow config ID
	 */
	uint32_t GetEyebrows() const { return m_Eyebrows; }

	/**
	 * Get the mouth of this character
	 * @return the mouth ID
	 */
	uint32_t GetMouth() const { return m_Mouth; }

	/**
	 * Gets the left hand color of this character
	 * @return the left hand color ID
	 */
	uint32_t GetLeftHand() const { return m_LeftHand; }

	/**
	 * Gets the right hand color of this character
	 * @return the right hand color ID
	 */
	uint32_t GetRightHand() const { return m_RightHand; }

	/**
	 * Sets the default shirt color for this character
	 * @param color the shirt color ID to set
	 */
	void SetShirtColor(uint32_t color) { m_ShirtColor = color; }

	/**
	 * Sets the default shirt style for this character
	 * @param style the shirt style ID to set
	 */
	void SetShirtStyle(uint32_t style) { m_ShirtStyle = style; }

	/**
	 * Sets the default pants color for this character
	 * @param color the pants color ID to set
	 */
	void SetPantsColor(uint32_t color) { m_PantsColor = color; }

	/**
	 * Sets the default hair color for this character
	 * @param color the hair color ID to set
	 */
	void SetHairColor(uint32_t color) { m_HairColor = color; }

	/**
	 * Sets the default hair style for this character
	 * @param style the hair style ID to set
	 */
	void SetHairStyle(uint32_t style) { m_HairStyle = style; }

	/**
	 * Sets the eyes config for this character
	 * @param eyes the eyes config ID to set
	 */
	void SetEyes(uint32_t eyes) { m_Eyes = eyes; }

	/**
	 * Sets the eyebrows config for this character
	 * @param eyebrows the eyebrows config ID to set
	 */
	void SetEyebrows(uint32_t eyebrows) { m_Eyebrows = eyebrows; }

	/**
	 * Sets the mouth config for this character
	 * @param mouth the mouth config ID to set
	 */
	void SetMouth(uint32_t mouth) { m_Mouth = mouth; }

	/**
	 * Sets the left hand color for this character
	 * @param color the left hand color ID to set
	 */
	void SetLeftHand(uint32_t leftHand) { m_LeftHand = leftHand; }

	/**
	 * Sets the right hand color for this character
	 * @param color the right hand color ID to set
	 */
	void SetRightHand(uint32_t rightHand) { m_RightHand = rightHand; }


	/**
	 * Whether this character has visited a certain zone
	 * @param mapID the ID of the zone to check for
	 * @return Whether the character has visited the provided zone
	 */
	bool HasBeenToWorld(LWOMAPID mapID) const;

	/**
	 * Gets the zone ID the character is currently in
	 * @return the zone ID the character is currently in
	 */
	uint32_t GetZoneID() const { return m_ZoneID; }

	/**
	 * Sets the zone ID the character is currently in
	 * @param id the zone ID to set
	 */
	void SetZoneID(uint32_t id) { m_ZoneID = id; }

	/**
	 * Gets the instance ID of the zone the character is currently in, for boss battles
	 * @return the instance ID of the zone the character is in
	 */
	uint32_t GetZoneInstance() const { return m_ZoneInstanceID; }

	/**
	 * Sets the zone instance ID the character is currently in
	 * @param instance the instance ID of the zone
	 */
	void SetZoneInstance(uint32_t instance) { m_ZoneInstanceID = instance; }

	/**
	 * Gets the clone ID of the zone the character is currently in, for properties
	 * @return the clone ID of the zone the character is in
	 */
	uint32_t GetZoneClone() const { return m_ZoneCloneID; }

	/**
	 * Sets the clone ID of the zone the character is currently in
	 * @param clone the clone ID of the zone
	 */
	void SetZoneClone(uint32_t clone) { m_ZoneCloneID = clone; }

	/**
	 * Gets the last zone the character was in, that was not an instance (=boss battle), to be able to send them back
	 * @return the zone ID of the last non-instance zone this character was in
	 */
	uint32_t GetLastNonInstanceZoneID() const { return m_LastNonInstanceZoneID; }

	/**
	 * Sets the last non instance zone ID for the character
	 * @param id the zone ID
	 */
	void SetLastNonInstanceZoneID(uint32_t id) { m_LastNonInstanceZoneID = id; }

	/**
	 * Gets the name of the scene that will play when the character lands in the next zone
	 * @return the name of the landing scene
	 */
	const std::string& GetTargetScene() const { return m_TargetScene; }

	/**
	 * Sets the name of the landing scene that will play when the player lands in the new zone
	 * NOTE: Generally set by launch pads before heading off to the next zone
	 * @param value the name of the landing scene to set
	 */
	void SetTargetScene(const std::string& value) { m_TargetScene = value; }

	/**
	 * Gets the starting position of the character (at spawn)
	 * @return the starting position of the character
	 */
	const NiPoint3& GetOriginalPos() const { return m_OriginalPosition; }

	/**
	 * Gets the starting rotation of the character (at spawn)
	 * @return the starting rotation of the character
	 */
	const NiQuaternion& GetOriginalRot() const { return m_OriginalRotation; }

	/**
	 * Gets the respawn point of the the character for a certain map
	 * @param map the map ID to get the respawn point for
	 * @return the respawn point of the character on the given map
	 */
	const NiPoint3& GetRespawnPoint(LWOMAPID map) const;

	/**
	 * Sets the respawn point of this character for a given map
	 * @param map the map to set the respawn point for
	 * @param point the point to set as respawn point on the given map
	 */
	void SetRespawnPoint(LWOMAPID map, const NiPoint3& point);

	/**
	 * Gets the GM level of the character
	 * @return the GM level
	 */
	int32_t GetGMLevel() const { return m_GMLevel; }

	/**
	 * Sets the GM level of the character
	 * @param value the GM level to set
	 */
	void SetGMLevel(uint8_t value) { m_GMLevel = value; }

	/**
	 * Gets the current amount of coins of the character
	 * @return the current amount of coins
	 */
	const int64_t GetCoins() const { return m_Coins; }

	/**
	 * Updates the current amount of coins of the character by a specified amount
	 * @param newCoins the amount of coins to update by
	 * @param coinSource The source of the loot
	 */
	void SetCoins(int64_t newCoins, eLootSourceType coinSource);

	/**
	 * Get the entity this character belongs to
	 * @return the entity this character belongs to
	 */
	Entity* GetEntity() const { return m_OurEntity; }

	/**
	 * Sets the entity this character belongs to
	 * @param entity the entity this character belongs to
	 */
	void SetEntity(Entity* entity) { m_OurEntity = entity; }

	/**
	 * Gets the current build mode of the character (on or off)
	 * @return the current build mode of the character
	 */
	bool GetBuildMode() const { return m_BuildMode; }

	/**
	 * Sets the current build mode for the character (either on or off)
	 * @param buildMode the build mode to set
	 */
	void SetBuildMode(bool buildMode);

	/**
	 * Gets the title of an announcement that a character made (reserved for GMs)
	 * @return the title of the announcement a character made
	 */
	const std::string& GetAnnouncementTitle() const { return m_AnnouncementTitle; }

	/**
	 * Sets the title of an announcement a character will make (reserved for GMs)
	 * @param value the title to set
	 */
	void SetAnnouncementTitle(const std::string& value) { m_AnnouncementTitle = value; }

	/**
	 * Gets the body of an announcement a character made (reserved for GMs)
	 * @return the body of the announcement
	 */
	const std::string& GetAnnouncementMessage() const { return m_AnnouncementMessage; }

	/**
	 * Sets the body of an annoucement to make (reserved for GMs)
	 * @param value the body of the announcement
	 */
	void SetAnnouncementMessage(const std::string& value) { m_AnnouncementMessage = value; }

	/**
	 * Called when the character has loaded into a zone
	 */
	void OnZoneLoad();

	/**
	 * Gets the permissions of the character, determining what actions a character may do
	 * @return the permissions for this character
	 */
	PermissionMap GetPermissionMap() const;

	/**
	 * Check if this character has a certain permission
	 * @param permission the ID of the permission to check for
	 * @return whether the character has the specified permission
	 */
	bool HasPermission(PermissionMap permission) const;

	/**
	 * Gets all the emotes this character has unlocked so far
	 * @return the emotes this character has unlocked
	 */
	const std::vector<int>& GetUnlockedEmotes() const { return m_UnlockedEmotes; }

	/**
	 * Unlocks an emote, adding it to the unlocked list, also updates the state for the client
	 * @param emoteID the ID of the emote to unlock
	 */
	void UnlockEmote(int emoteID);

	/**
	 * Sets a flag for the character, indicating certain parts of the game that have been interacted with. Not to be
	 * confused with the permissions
	 * @param flagId the ID of the flag to set
	 * @param value the value to set for the flag
	 */
	void SetPlayerFlag(uint32_t flagId, bool value);

	/**
	 * Gets the value for a certain character flag
	 * @param flagId the ID of the flag to get a value for
	 * @return the value of the flag given the ID (the default is false, obviously)
	 */
	bool GetPlayerFlag(uint32_t flagId) const;

	/**
	 * Notifies the character that they're now muted
	 */
	void SendMuteNotice() const;

	/**
	 * Sets any flags that are meant to have been set that may not have been set due to them being
	 * missing in a previous patch.
	 */
	void SetRetroactiveFlags();

	/**
	 * Get the equipped items for this character, only used for character creation
	 * @return the equipped items for this character on world load
	 */
	const std::vector<LOT>& GetEquippedItems() const { return m_EquippedItems; }

	/**
	 * @brief Get the flying state
	 * @return value of the flying state 
	*/
	bool GetIsFlying() { return m_IsFlying; }

	/**
	 * @brief Set the value of the flying state
	 * @param isFlying the flying state
	*/
	void SetIsFlying(bool isFlying) { m_IsFlying = isFlying; }

private:
	/**
	 * The ID of this character. First 32 bits of the ObjectID.
	 */
	uint32_t m_ID;

	/**
	 * The 64-bit unique ID used in the game.
	 */
	LWOOBJID m_ObjectID;

	/**
	 * The user that owns this character.
	 */
	User* m_ParentUser;

	/**
	 * If the character is in game, this is the entity that it represents, else nullptr.
	 */
	Entity* m_OurEntity;

	/**
	 * 0-9, the Game Master level of this character.
	 *
	 * @see eGameMasterLevel
	 */
	int32_t m_GMLevel;

	/**
	 * Bitmap of permission attributes this character has.
	 */
	PermissionMap m_PermissionMap;

	/**
	 * The default name of this character
	 */
	std::string m_Name;

	/**
	 * The custom name of the character
	 */
	std::string m_UnapprovedName;

	/**
	 * Whether the custom name of this character is rejected
	 */
	bool m_NameRejected;

	/**
	 * The current amount of coins of this character
	 */
	int64_t m_Coins;

	/**
	 * Whether the character is building
	 */
	bool m_BuildMode;

	/**
	 * The items equipped by the character on world load
	 */
	std::vector<LOT> m_EquippedItems;

	/**
	 * The default shirt color of the character
	 */
	uint32_t m_ShirtColor = 0;

	/**
	 * The default shirt style of the character
	 */
	uint32_t m_ShirtStyle = 0;

	/**
	 * The default pants color of the character
	 */
	uint32_t m_PantsColor = 1;

	/**
	 * The default hair color of the character
	 */
	uint32_t m_HairColor = 0;

	/**
	 * The default hair style of the character
	 */
	uint32_t m_HairStyle = 0;

	/**
	 * The eyes style of the character
	 */
	uint32_t m_Eyes = 1;

	/**
	 * The eyebrow style of the character
	 */
	uint32_t m_Eyebrows = 33;

	/**
	 * The mouth style of the character
	 */
	uint32_t m_Mouth = 8;

	/*
	 * The left hand ID of the character
	 * NOTE: This might just be client stack garbage.
	 */
	uint32_t m_LeftHand = 23571472;

	/**
	 * The right hand ID of the character
	 * NOTE: This might just be client stack garbage.
	 */
	uint32_t m_RightHand = 23124164;

	/**
	 * The emotes unlocked by this character
	 */
	std::vector<int> m_UnlockedEmotes;

	/**
	 * The ID of the properties of this character
	 */
	uint32_t m_PropertyCloneID;

	/**
	 * The XML data for this character, stored as string
	 */
	std::string m_XMLData;

	/**
	 * The last zone visited by the character that was not an instance zone
	 */
	uint32_t m_LastNonInstanceZoneID = 0;

	/**
	 * The ID of the zone the character is currently in
	 */
	uint32_t m_ZoneID = 0;

	/**
	 * The instance ID of the zone the character is currently in (for boss battles)
	 */
	uint32_t m_ZoneInstanceID = 0;

	/**
	 * The clone ID of the zone the character is currently in (for properties)
	 */
	uint32_t m_ZoneCloneID = 0;

	/**
	 * The last time this character logged in
	 */
	uint64_t m_LastLogin;

	/**
	 * The gameplay flags this character has (not just true values)
	 */
	std::unordered_map<uint32_t, uint64_t> m_PlayerFlags;

	/**
	 * The character XML belonging to this character
	 */
	tinyxml2::XMLDocument* m_Doc;

	/**
	 * Title of an announcement this character made (reserved for GMs)
	 */
	std::string m_AnnouncementTitle;

	/**
	 * The body of an announcement this character made (reserved for GMs)
	 */
	std::string m_AnnouncementMessage;

	/**
	 * The spawn position of this character when loading in
	 */
	NiPoint3 m_OriginalPosition;

	/**
	 * The spawn rotation of this character when loading in
	 */
	NiQuaternion m_OriginalRotation;

	/**
	 * The respawn points of this character, per world
	 */
	std::map<LWOMAPID, NiPoint3> m_WorldRespawnCheckpoints;

	/**
	 * The scene where the player will land.
	 * Set by the launchpad the player used to get to the current world.
	 */
	std::string m_TargetScene;

	/**
	 * Bool that tracks the flying state of the user.
	*/
	bool m_IsFlying = false;

	/**
	 * Queries the character XML and updates all the fields of this object
	 * NOTE: quick as there's no DB lookups
	 */
	void DoQuickXMLDataParse();
};

#endif // CHARACTER_H
