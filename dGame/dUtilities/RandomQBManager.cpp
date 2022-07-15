#include "RandomQBManager.h"


RandomQBManager::RandomQBManager() {
    this->m_SpawnerCounter = 0;
    this->m_QBCounter = 0;
    this->m_RandomQBList = std::vector<RandomQuickBuild>();
    this->m_RandomQuickBuildLocations = std::vector<RandomQuickBuildLocation>();
    this->m_CallbackTimers = std::vector<EntityCallbackTimer*>();
}

RandomQBManager::~RandomQBManager() {

}

void RandomQBManager::ApplyQBToLocation(uint32_t qbId, uint32_t locId) {
    RandomQuickBuild* qb = this->GetQBById(qbId);
    RandomQuickBuildLocation* loc = this->GetLocById(locId);

    if (qb->qbSizeName != loc->sizeName) {
        return;
    }

    loc->occupiedBy = qb->id;

    NiQuaternion rotation = loc->rotation;

    if (qb->rotationOverrides.find(loc->name) != qb->rotationOverrides.end()) {
        rotation = qb->rotationOverrides[loc->name];
    }

    EntityInfo info{};
    info.lot = qb->lot;
    info.pos = loc->location;
    info.rot = rotation;
    info.spawnerID = EntityManager::Instance()->GetZoneControlEntity()->GetObjectID();
    info.settings = {};

    auto* entity = EntityManager::Instance()->CreateEntity(info);
    EntityManager::Instance()->ConstructEntity(entity);

    auto* rebuildComp = entity->GetComponent<RebuildComponent>();

    if (rebuildComp) {
        if (rebuildComp->GetDoReset()) {
            rebuildComp->SetDoReset(false);
        }
    }

    loc->spawnedObject = entity->GetObjectID();

    EntityCallbackTimer* timer = new EntityCallbackTimer(loc->length, [this, loc]() {
        this->ClearLocation(loc->id);
    });

    EntityCallbackTimer* restartTimer = new EntityCallbackTimer(loc->length + loc->lengthBeforeNextSpawn, [this, loc]() {
        this->ChanceALocation(loc->id);
    });

    m_CallbackTimers.push_back(timer);
    m_CallbackTimers.push_back(restartTimer);
}

void RandomQBManager::ChanceALocation(uint32_t locId) {
    auto* location = this->GetLocById(locId);
    auto chanceResult = GeneralUtils::GenerateRandomNumber<float>(0, 100);
    if (chanceResult <= location->chance) {
        auto qbs = this->GetQBsBySize(location->sizeName);

        if (qbs.size() == 0) {
            EntityCallbackTimer* restartTimer = new EntityCallbackTimer(location->lengthBeforeNextSpawn, [&]() {
                this->ChanceALocation(locId);
            });
            m_CallbackTimers.push_back(restartTimer);
            return;
        }

        auto* qb = qbs.at(GeneralUtils::GenerateRandomNumber<int32_t>(0, qbs.size() - 1));

        if (qbs.size() > 1) {
            if (qb->id == location->lastId) {
                while (qb->id == location->lastId) {
                    qb = qbs.at(GeneralUtils::GenerateRandomNumber<int32_t>(0, qbs.size() - 1));
                }
            }
        }

        Game::logger->Log("RandomQBManager", "Chanced for location: %s \n", location->name.c_str());

        this->ApplyQBToLocation(qb->id, location->id);
    } else {
        EntityCallbackTimer* restartTimer = new EntityCallbackTimer(location->lengthBeforeNextSpawn, [&]() {
            this->ChanceALocation(locId);
        });
        m_CallbackTimers.push_back(restartTimer);
    }
} 

void RandomQBManager::ClearLocation(uint32_t locId) {
    auto* location = this->GetLocById(locId);

    GameMessages::SendDieNoImplCode(EntityManager::Instance()->GetEntity(location->spawnedObject), LWOOBJID_EMPTY, LWOOBJID_EMPTY, eKillType::VIOLENT, u"", 0.0f, 0.0f, 0.0f, false, true);
    
    EntityManager::Instance()->DestroyEntity(location->spawnedObject);
    
    location->lastId = location->occupiedBy;
    location->occupiedBy = -1;
    location->spawnedObject = LWOOBJID_EMPTY;

    Game::logger->Log("RandomQBManager", "Cleared location: %s \n", location->name.c_str());
}

std::vector<RandomQuickBuild*> RandomQBManager::GetQBsBySize(std::string size, bool unused) {
    auto returnValue = std::vector<RandomQuickBuild*>();

    for (auto& item : this->m_RandomQBList) {
        bool moveOn = false;
        for (const auto& loc : this->m_RandomQuickBuildLocations) {
            if (loc.occupiedBy == item.id) {
                moveOn = unused;
            }
        }

        if (moveOn) continue;

        if (item.qbSizeName == size) {
            returnValue.push_back(&item);
        }
    }

    return returnValue;
}

RandomQuickBuild* RandomQBManager::GetQBById(uint32_t id) {
    for (auto& item : this->m_RandomQBList) {
        if (item.id == id) {
            return &item;
        }
    }
}

RandomQuickBuildLocation* RandomQBManager::GetLocById(uint32_t id) {
    for (auto& item : this->m_RandomQuickBuildLocations) {
        if (item.id == id) {
            return &item;
        }
    }
}

void RandomQBManager::Load(uint32_t zoneId) {
    std::string file = "./qbs/" + std::to_string((uint32_t)zoneId) + ".xml";

    std::ifstream xmlFile(file);

    if (!xmlFile.is_open()) {
        if (zoneId != 0) {
            Game::logger->Log("RandomQBManager", "Failed to load random quick build data for world\n"); 
        } else {
            Game::logger->Log("RandomQBManager", "Failed to load common random quick build data\n"); 
        }

        return;
    }

    std::string xml((std::istreambuf_iterator<char>(xmlFile)), std::istreambuf_iterator<char>());
    
    // Parse the XML
    tinyxml2::XMLDocument doc;
    doc.Parse(xml.c_str(), xml.size());

    auto* qbs = doc.FirstChildElement("qbs");
    auto* locs = doc.FirstChildElement("locs");

    if (qbs) {
        for (auto* qb = qbs->FirstChildElement("qb"); qb; qb = qb->NextSiblingElement()) {
            RandomQuickBuild qbData{};

            if (!qb->Attribute("name") || !qb->Attribute("lot") || !qb->Attribute("size")) {\
                qb->Attribute("poggers");
                continue;
            }

            qbData.name = std::string(qb->Attribute("name"));
            uint32_t lot;
            bool validLot = GeneralUtils::TryParse(std::string(qb->Attribute("lot")), lot);
            if (!validLot) {
                continue;
            }
            qbData.lot = lot;

            qbData.qbSizeName = qb->Attribute("size");

            auto* overrides = qb->FirstChildElement("overrides");
            if (overrides) {
                for (auto* override = overrides->FirstChildElement("override"); override; override = overrides->NextSiblingElement()) {
                    if (!override->Attribute("name") || !override->Attribute("x") || !override->Attribute("y") || !override->Attribute("z") || !override->Attribute("w")) {
                        continue;
                    }

                    auto* name = override->Attribute("name");

                    float x, y, z, w;
                    bool xValid, yValid, zValid, wValid;
                    xValid = GeneralUtils::TryParse(std::string(override->Attribute("x")), x);
                    yValid = GeneralUtils::TryParse(std::string(override->Attribute("y")), y);
                    zValid = GeneralUtils::TryParse(std::string(override->Attribute("z")), z);
                    wValid = GeneralUtils::TryParse(std::string(override->Attribute("w")), w);

                    if (xValid && yValid && zValid && wValid) {
                        auto quaternion = NiQuaternion(w, x, y, z);

                        qbData.rotationOverrides[std::string(name)] = quaternion;
                    }
                }
            }

            qbData.id = m_QBCounter;
            m_QBCounter += 1;
            this->m_RandomQBList.push_back(qbData);
        }
    }

    Game::logger->Log("RandomQBManager", "Loaded quick builds for zone " + std::to_string(zoneId) + "\n");

    if (locs) {
        for (auto* loc = locs->FirstChildElement("loc"); loc; loc = loc->NextSiblingElement()) {
            RandomQuickBuildLocation qbLocData{};

            if (!loc->Attribute("name") ||
                !loc->Attribute("size") ||
                !loc->Attribute("x") ||
                !loc->Attribute("y") ||
                !loc->Attribute("z") ||
                !loc->Attribute("rx") ||
                !loc->Attribute("ry") ||
                !loc->Attribute("rz") ||
                !loc->Attribute("rw")) 
            {
                continue;
            }

            qbLocData.name = std::string(loc->Attribute("name"));
            qbLocData.sizeName = std::string(loc->Attribute("size"));
            
            float x, y, z;
            bool xValid, yValid, zValid;
            xValid = GeneralUtils::TryParse(std::string(loc->Attribute("x")), x);
            yValid = GeneralUtils::TryParse(std::string(loc->Attribute("y")), y);
            zValid = GeneralUtils::TryParse(std::string(loc->Attribute("z")), z);

            float rx, ry, rz, rw;
            bool rxValid, ryValid, rzValid, rwValid;
            rxValid = GeneralUtils::TryParse(std::string(loc->Attribute("rx")), rx);
            ryValid = GeneralUtils::TryParse(std::string(loc->Attribute("ry")), ry);
            rzValid = GeneralUtils::TryParse(std::string(loc->Attribute("rz")), rz);
            rwValid = GeneralUtils::TryParse(std::string(loc->Attribute("rw")), rw);

            if (xValid && yValid && zValid && rxValid && ryValid && rzValid && rwValid) {
                auto pos = NiPoint3(x, y, z);
                auto rot = NiQuaternion(rx, ry, rz, rw);

                qbLocData.location = pos;
                qbLocData.rotation = rot;                
            } else {
                continue;
            }

            if (loc->Attribute("length")) {
                uint32_t length;
                auto valid = GeneralUtils::TryParse(loc->Attribute("length"), length);
                qbLocData.length = length;
            }

            if (loc->Attribute("wait")) {
                uint32_t wait;
                auto valid = GeneralUtils::TryParse(loc->Attribute("wait"), wait);
                qbLocData.lengthBeforeNextSpawn = wait;
            }

            if (loc->Attribute("chance")) {
                float chance;
                auto valid = GeneralUtils::TryParse(loc->Attribute("chance"), chance);
                qbLocData.chance = chance;
            }

            qbLocData.id = m_SpawnerCounter;
            m_SpawnerCounter += 1;
            this->m_RandomQuickBuildLocations.push_back(qbLocData);
        }
    }

    // Random Startup
    for (auto& item : m_RandomQuickBuildLocations) {
        this->ChanceALocation(item.id);
    }
}

void RandomQBManager::Update(float deltaTime) {
    uint32_t i = 0;
    while (i < m_CallbackTimers.size()) {
        m_CallbackTimers[i]->Update(deltaTime);
        if (m_CallbackTimers[i]->GetTime() <= 0) {
            m_CallbackTimers[i]->GetCallback()();
            delete m_CallbackTimers[i];
            m_CallbackTimers.erase(m_CallbackTimers.begin() + i);
        } else {
            i++;
        }
    }
}