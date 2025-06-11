#pragma once
#include "dZMCommon.h"
#include "WorldConfig.h"
#include "Zone.h"
#include "Spawner.h"
#include <map>

class dZoneManager {
public:
	enum class dZoneNotifier {
		SpawnedObjectDestroyed,
		SpawnedChildObjectDestroyed, //Used for when an object (like a stromling) needs to notify the spawner to respawn a new enemy.
		ReloadZone, //Forces the server and all connects clients to reload the map
		UserJoined,
		UserMoved,
		InvalidNotifier
	};

private:
	/**
	 * Reads the WorldConfig from the CDClientDatabase into memory
	 */
	void LoadWorldConfig();

public:
	void Initialize(const LWOZONEID& zoneID);
	~dZoneManager();

	/* Gets a pointer to the currently loaded zone. */
	Zone* GetZoneMut() const;
	const Zone* GetZone() const { return GetZoneMut(); };
	bool HasZone() const { return m_pZone != nullptr; };
	void LoadZone(const LWOZONEID& zoneID); //Discard the current zone (if any) and loads a new zone.

	/* Adds a spawner to the zone with the specified ID. */
	void AddSpawner(LWOOBJID id, Spawner* spawner);
	const LWOZONEID& GetZoneID() const;

	/* Creates a new spawner. Returns the finalized ID for the created spawner since some bits may be set to get it to function. */
	LWOOBJID MakeSpawner(SpawnerInfo info);
	Spawner* GetSpawner(LWOOBJID id);
	void RemoveSpawner(LWOOBJID id);
	std::vector<Spawner*> GetSpawnersByName(const std::string& spawnerName);
	std::vector<Spawner*> GetSpawnersInGroup(const std::string& group);
	void Update(float deltaTime);
	Entity* GetZoneControlObject() { return m_ZoneControlObject; }
	bool GetPlayerLoseCoinOnDeath() { return m_PlayerLoseCoinsOnDeath; }
	bool GetDisableSaveLocation() { return m_DisableSaveLocation; }
	bool GetMountsAllowed() { return m_MountsAllowed; }
	bool GetPetsAllowed() { return m_PetsAllowed; }

	/* Gets the starting ID for missions in the player UI so they are ordered properly and show in the order accepted by the player. */
	uint32_t GetUniqueMissionIdStartingValue();
	bool CheckIfAccessibleZone(LWOMAPID zoneID);

	// The world config should not be modified by a caller.
	const WorldConfig& GetWorldConfig() {
		if (!m_WorldConfig) LoadWorldConfig();
		return m_WorldConfig.value();
	};

private:
	/**
	 * The starting unique mission ID.
	 */
	uint32_t m_UniqueMissionIdStart = 0;

	Zone* m_pZone = nullptr;
	LWOZONEID m_ZoneID;
	bool m_PlayerLoseCoinsOnDeath = false;
	bool m_DisableSaveLocation = false;
	bool m_MountsAllowed = true;
	bool m_PetsAllowed = true;
	std::map<LWOOBJID, Spawner*> m_Spawners;
	std::optional<WorldConfig> m_WorldConfig = std::nullopt;

	Entity* m_ZoneControlObject = nullptr;
};
