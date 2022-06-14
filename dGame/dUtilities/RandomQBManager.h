#pragma once

#include <vector>
#include <unordered_map>
#include <string>
#include <fstream>

#include "tinyxml2.h"

#include "dCommonVars.h"
#include "NiPoint3.h"
#include "NiQuaternion.h"
#include "Game.h"
#include "dLogger.h"
#include "GeneralUtils.h"
#include "EntityCallbackTimer.h"
#include "EntityInfo.h"
#include "EntityManager.h"

struct RandomQuickBuild {
    uint32_t id; // Handed out at runtime
    std::string name;
    uint32_t lot;
    std::string qbSizeName;

    // Location overrides
    std::unordered_map<std::string, NiQuaternion> rotationOverrides = {};
};

struct RandomQuickBuildLocation {
    uint32_t id; // Handed out at runtime
    std::string name;
    std::string sizeName;
    NiPoint3 location;
    NiQuaternion rotation;
    uint32_t length = 10;
    uint32_t lengthBeforeNextSpawn = 1;
    uint32_t chance = 100;

    uint32_t occupiedBy = -1;
    LWOOBJID spawnedObject = LWOOBJID_EMPTY;
    uint32_t lastId = -1;
};


class RandomQBManager {
  public:
    RandomQBManager();
    ~RandomQBManager();

    void Load(uint32_t zoneId = 0);
    void Update(float deltaTime);

    std::vector<RandomQuickBuild*> GetQBsBySize(std::string size, bool unused = true);
    RandomQuickBuild* GetQBById(uint32_t id);
    RandomQuickBuildLocation* GetLocById(uint32_t id);

    void ApplyQBToLocation(uint32_t qbId, uint32_t locId);
    void ChanceALocation(uint32_t locId);
    void ClearLocation(uint32_t locId);

  private:
    uint32_t m_QBCounter;
    uint32_t m_SpawnerCounter;
    std::vector<RandomQuickBuild> m_RandomQBList;
    std::vector<RandomQuickBuildLocation> m_RandomQuickBuildLocations;
    std::vector<EntityCallbackTimer*> m_CallbackTimers;
};