#ifndef CHARACTERCOMPONENT_H
#define CHARACTERCOMPONENT_H

#include "dCommonVars.h"
#include "RakNetTypes.h"
#include "Character.h"
#include "Component.h"
#include "Item.h"
#include <string>
#include "CDMissionsTable.h"
#include "tinyxml2.h"

/**
 * The statistics that can be achieved per zone
 */
struct ZoneStatistics {
    uint64_t m_AchievementsCollected;
    uint64_t m_BricksCollected;
    uint64_t m_CoinsCollected;
    uint64_t m_EnemiesSmashed;
    uint64_t m_QuickBuildsCompleted;
};

/**
 * The IDs of each of the possible statistics
 */
enum StatisticID {
    CurrencyCollected = 1,
    BricksCollected,
    SmashablesSmashed,
    QuickBuildsCompleted,
    EnemiesSmashed,
    RocketsUsed,
    MissionsCompleted,
    PetsTamed,
    ImaginationPowerUpsCollected,
    LifePowerUpsCollected,
    ArmorPowerUpsCollected,
    MetersTraveled,
    TimesSmashed,
    TotalDamageTaken,
    TotalDamageHealed,
    TotalArmorRepaired,
    TotalImaginationRestored,
    TotalImaginationUsed,
    DistanceDriven,
    TimeAirborneInCar,
    RacingImaginationPowerUpsCollected,
    RacingImaginationCratesSmashed,
    RacingCarBoostsActivated,
    RacingTimesWrecked,
    RacingSmashablesSmashed,
    RacesFinished,
    FirstPlaceRaceFinishes,
};

/**
 * Represents a character, including their rockets and stats
 */
class CharacterComponent : public Component {
public:
	static const uint32_t ComponentType = COMPONENT_TYPE_CHARACTER;
	
    CharacterComponent(Entity* parent, Character* character);
    ~CharacterComponent() override;
	
    void LoadFromXML();
	void UpdateXml(tinyxml2::XMLDocument* doc) override;

    void Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags);

    /**
     * Updates the rocket configuration using a LOT string separated by commas
     * @param config the rocket config to use
     */
	void SetLastRocketConfig(std::u16string config);

	/**
	 * Find a player's rocket
	 * @param player the entity that triggered the event
	 * @return rocket
	 */
	Item* GetRocket(Entity* player);

	/**
	 * Equip a player's rocket
	 * @param player the entity that triggered the event
	 * @return rocket
	 */
	Item* RocketEquip(Entity* player);

	/**
	 * Find a player's rocket and unequip it
	 * @param player the entity that triggered the event
	 */
	void RocketUnEquip(Entity* player);

    /**
     * Gets the current level of the entity
     * @return the current level of the entity
     */
	const uint32_t GetLevel() const { return m_Level; }

    /**
     * Sets the level of the entity
     * @param level the level to set
     */
	void SetLevel(uint32_t level) { m_Level = level; }

    /**
     * Gets the universe score of the entity
     * @return the universe score of the entity
     */
	const int64_t GetUScore() const { return m_Uscore; }

    /**
     * Sets the universe score for this entity
     * @param uscore the universe score to set
     */
	void SetUScore(int64_t uscore) { m_Uscore = uscore; }

    /**
     * Gets the current activity that the character is partaking in, see ScriptedActivityComponent for more details
     * @return the current activity that the character is partaking in
     */
	const uint32_t GetCurrentActivity() const { return m_CurrentActivity; }

    /**
     * Set the current activity of the character, see ScriptedActivityComponent for more details
     * @param currentActivity the activity to set
     */
	void SetCurrentActivity(uint32_t currentActivity) { m_CurrentActivity = currentActivity; m_DirtyCurrentActivity = true; }

    /**
     * Gets if the entity is currently racing
     * @return whether the entity is currently racing
     */
	const bool GetIsRacing() const { return m_IsRacing; }

    /**
     * Sets the state of whether the character is racing
     * @param isRacing whether the character is racing
     */
	void SetIsRacing(bool isRacing) { m_IsRacing = isRacing; }

    /**
     * Gets whether this character has PvP enabled, allowing combat between players
     * @return
     */
	bool GetPvpEnabled() const;

    /**
     * Returns the characters lifetime reputation
     * @return The lifetime reputation of this character.
     */
    int64_t GetReputation() { return m_Reputation; };

    /**
     * Sets the lifetime reputation of the character to newValue
     * @param newValue the value to set reputation to
     */
    void SetReputation(int64_t newValue) { m_Reputation = newValue; };

    /**
     * Sets the current value of PvP combat being enabled
     * @param value whether to enable PvP combat
     */
	void SetPvpEnabled(bool value);

    /**
     * Gets the object ID of the rocket that was last used, allowing it to be rendered on launch pads
     * @return the object ID of the rocket that was last used, if available
     */
	LWOOBJID GetLastRocketItemID() const { return m_LastRocketItemID; }

    /**
     * Sets the object ID of the last used rocket
     * @param lastRocketItemID the object ID of the last used rocket
     */
	void SetLastRocketItemID(LWOOBJID lastRocketItemID) { m_LastRocketItemID = lastRocketItemID; }

	/**
	 * Gets the object ID of the mount item that is being used
	 * @return the object ID of the mount item that is being used
	 */
	LWOOBJID GetMountItemID() const { return m_MountItemID; }

	/**
	 * Sets the object ID of the mount item that is being used
	 * @param m_MountItemID the object ID of the mount item that is being used
	 */
	void SetMountItemID(LWOOBJID mountItemID) { m_MountItemID = mountItemID; }

    /**
     * Gives the player rewards for the last level that they leveled up from
     */
	void HandleLevelUp();

    /**
     * Gets the name of this character
     * @return the name of this character
     */
	std::string GetName() const { return m_Character->GetName(); }

    /**
     * Sets the GM level of the character, should be called in the entity. Here it's set for serialization
     * @param gmlevel the gm level to set
     */
	void SetGMLevel(int gmlevel);

    /**
     * Initializes the player statistics from the string stored in the XML
     * @param statisticsString the string to parse
     */
	void InitializeStatisticsFromString(const std::string& statisticsString);

    /**
     * Initializes all the statistics with empty stats when there's no stats available up until that point
     */
	void InitializeEmptyStatistics();

    /**
     * Turns character statistics into a stats string
     * @return the statistics of the character as a string, in order, split by semicolon (;)
     */
	std::string StatisticsToString() const;

	/**
	 * Updates the statistics for when a user completes a mission
	 * @param mission the mission info to track
	 */
    void TrackMissionCompletion(bool isAchievement);

	/**
	 * Handles statistics related to collecting heart flags and imagination bricks
	 * @param lot the lot of the object that was collected
	 */
	void TrackLOTCollection(LOT lot);

	/**
	 * Handles a change in health and updates the statistics
	 * @param health the health delta
	 */
	void TrackHealthDelta(int32_t health);

	/**
	 * Handles a change in imagination and updates the statistics
	 * @param imagination the imagination delta
	 */
	void TrackImaginationDelta(int32_t imagination);

	/**
	 * Handles a change in armor and updates the statistics
	 * @param armor the armor delta
	 */
	void TrackArmorDelta(int32_t armor);

	/**
	 * Handles completing a rebuild by updating the statistics
	 */
	void TrackRebuildComplete();

	/**
	 * Tracks a player completing the race, also updates stats
	 * @param won whether the player won the race
	 */
	void TrackRaceCompleted(bool won);

	/**
	 * Tracks an updated position for a player
	 */
	void TrackPositionUpdate(const NiPoint3& newPosition);

	/**
	 * Handles a zone statistic update
	 * @param zoneID the zone that the stat belongs to
	 * @param name the name of the stat
	 * @param value the delta update for the stat
	 */
	void HandleZoneStatisticsUpdate(LWOMAPID zoneID, const std::u16string& name, int32_t value);

	/**
	 * Allows one to generically update a statistic
	 * @param updateID the 1-indexed ID of the statistic in the order of definition below
	 * @param updateValue the value to update the statistic with
	 */
	void UpdatePlayerStatistic(StatisticID updateID, uint64_t updateValue = 1);

    /**
     * Add a venture vision effect to the player minimap.
     */
    void AddVentureVisionEffect(std::string ventureVisionType);

    /**
     * Remove a venture vision effect from the player minimap.
     * When an effect hits 0 active effects, it is deactivated.
     */
    void RemoveVentureVisionEffect(std::string ventureVisionType);

    /**
     * Update the client minimap to reveal the specified factions
     */
    void UpdateClientMinimap(bool showFaction, std::string ventureVisionType) const;

    /**
     * Character info regarding this character, including clothing styles, etc.
     */
    Character* m_Character;
private:
    
    /** 
     * The map of active venture vision effects
     */
    std::map<std::string, uint32_t> m_ActiveVentureVisionEffects;

    /**
     * Whether this character is racing
     */
	bool m_IsRacing;

    /**
     * Possessible type, used by the shooting gallery
     */
	uint8_t m_PossessableType = 1;

    /**
     * Level of the entity
     */
	uint32_t m_Level;

    /**
     * Universe score of the entity
     */
	int64_t m_Uscore;

    /**
     * The lifetime reputation earned by the entity
     */
    int64_t m_Reputation;

    /**
     * Whether the character is landing by rocket
     */
	bool m_IsLanding;

    /**
     * The configuration of the last used rocket, essentially a string of LOTs separated by commas
     */
	std::u16string m_LastRocketConfig;

    /**
     * Whether the GM info has been changed
     */
	bool m_DirtyGMInfo = false;

    /**
     * Whether PvP is enabled for this entity
     */
	bool m_PvpEnabled;

    /**
     * Whether this entity is a GM
     */
	bool m_IsGM;

    /**
     * The current GM level of this character (anything > 0 counts as a GM)
     */
	unsigned char m_GMLevel;

    /**
     * Whether the character has HF enabled
     */
	bool m_EditorEnabled;

    /**
     * The level of the character in HF
     */
	unsigned char m_EditorLevel;

    /**
     * Whether the currently active activity has been changed
     */
	bool m_DirtyCurrentActivity = false;

    /**
     * The ID of the curently active activity
     */
	int m_CurrentActivity;

    /**
     * Whether the social info has been changed
     */
	bool m_DirtySocialInfo = false;

    /**
     * The guild this character is in
     */
	LWOOBJID m_GuildID;

    /**
     * The name of the guild this character is in
     */
	std::u16string m_GuildName;

    /**
     * Whether this character is a lego club member
     */
	bool m_IsLEGOClubMember;

    /**
     * The country code that the character is from
     */
	int m_CountryCode;

    /**
     * Returns whether the landing animation is enabled for a certain zone
     * @param zoneID the zone to check for
     * @return whether the landing animation is enabled for that zone
     */
	bool LandingAnimDisabled(int zoneID);

    /**
     * Returns the statistics for a certain statistics ID, from a statistics string
     * @param split the statistics string to look in
     * @param index the statistics ID in the string
     * @return the integer value of this statistic, parsed from the string
     */
	static uint64_t GetStatisticFromSplit(std::vector<std::string> split, uint32_t index);

    /**
     * Gets all the statistics for a certain map, if it doesn't exist, it creates empty stats
     * @param mapID the ID of the zone to get statistics for
     * @return the statistics for the zone
     */
	ZoneStatistics& GetZoneStatisticsForMap(const LWOMAPID mapID);

    /**
     * The last time we saved this character, used to update the total time played
     */
    time_t m_LastUpdateTimestamp;

    /**
     * The total time the character has played, in MS
     */
    uint64_t m_TotalTimePlayed;

    /**
     * The total amount of currency collected by this character
     */
    uint64_t m_CurrencyCollected;

    /**
     * The total amount of bricks collected by this character
     */
    uint64_t m_BricksCollected;

    /**
     * The total amount of entities smashed by this character
     */
    uint64_t m_SmashablesSmashed;

    /**
     * The total amount of quickbuilds completed by this character
     */
    uint64_t m_QuickBuildsCompleted;

    /**
     * The total amount of enemies killd by this character
     */
    uint64_t m_EnemiesSmashed;

    /**
     * The total amount of rockets used by this character
     */
    uint64_t m_RocketsUsed;

    /**
     * The total amount of missions completed by this character
     */
    uint64_t m_MissionsCompleted;

    /**
     * The total number of pets tamed by this character
     */
    uint64_t m_PetsTamed;

    /**
     * The total amount of imagination powerups collected by this character, this includes the ones in racing
     */
    uint64_t m_ImaginationPowerUpsCollected;

    /**
     * The total amount of life powerups collected (note: not the total amount of life gained)
     */
    uint64_t m_LifePowerUpsCollected;

    /**
     * The total amount of armor powerups collected (note: not the total amount of armor gained)
     */
    uint64_t m_ArmorPowerUpsCollected;

    /**
     * Total amount of meters traveled by this character
     */
    uint64_t m_MetersTraveled;

    /**
     * Total amount of times this character was smashed, either by other entities or by going out of bounds
     */
    uint64_t m_TimesSmashed;

    /**
     * The total amount of damage inflicted on this character
     */
    uint64_t m_TotalDamageTaken;

    /**
     * The total amount of damage healed by this character (excludes armor polish, etc)
     */
    uint64_t m_TotalDamageHealed;

    /**
     * Total amount of armor repaired by this character
     */
    uint64_t m_TotalArmorRepaired;

    /**
     * Total amount of imagination resored by this character
     */
    uint64_t m_TotalImaginationRestored;

    /**
     * Total amount of imagination used by this character
     */
    uint64_t m_TotalImaginationUsed;

    /**
     * Amount of distance driven, mutually exclusively tracked to meters travelled based on whether the charcter
     * is currently driving
     */
    uint64_t m_DistanceDriven;

    /**
     * Time airborne in a car, currently untracked.
     * Honestly, who even cares about this.
     */
    uint64_t m_TimeAirborneInCar;

    /**
     * Amount of imagination powerups found on racing tracks being collected, generally triggered by scripts
     */
    uint64_t m_RacingImaginationPowerUpsCollected;

    /**
     * Total amount of racing imagination crates smashed, generally tracked by scripts
     */
    uint64_t m_RacingImaginationCratesSmashed;

    /**
     * The amount of times this character triggered a car boost
     */
    uint64_t m_RacingCarBoostsActivated;

    /**
     * The amount of times a car of this character was wrecked
     */
    uint64_t m_RacingTimesWrecked;

    /**
     * The amount of entities smashed by the character while driving
     */
    uint64_t m_RacingSmashablesSmashed;

    /**
     * The total amount of races completed by this character
     */
    uint64_t m_RacesFinished;

    /**
     * The total amount of races won by this character
     */
    uint64_t m_FirstPlaceRaceFinishes;

    /**
     * Special stats which are tracked per zone
     */
    std::map<LWOMAPID, ZoneStatistics> m_ZoneStatistics {};

	/**
	 * ID of the last rocket used
	 */
	LWOOBJID m_LastRocketItemID = LWOOBJID_EMPTY;

	/**
	 * Mount Item ID
	 */
	LWOOBJID m_MountItemID = LWOOBJID_EMPTY;
};

#endif // CHARACTERCOMPONENT_H
