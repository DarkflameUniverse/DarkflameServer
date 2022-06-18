#pragma once

#include "../GameMessage.h"

#include "EntityManager.h"
#include "dLogger.h"
#include "Entity.h"

class ActivityStateChangeRequest : public GameMessage {
public:
    constexpr static GAME_MSG GetId() { return GAME_MSG_ACTIVITY_STATE_CHANGE_REQUEST; }

    LWOOBJID objectID;
    int32_t value1;
    int32_t value2;
    uint32_t stringValueLength;
    std::u16string stringValue;

    void Deserialize(RakNet::BitStream* inStream) override {
        inStream->Read<LWOOBJID>(objectID);
        inStream->Read<int32_t>(value1);
        inStream->Read<int32_t>(value2);
        inStream->Read<uint32_t>(stringValueLength);
        for (uint32_t i = 0; i < stringValueLength; ++i) {
            uint16_t character;
            inStream->Read(character);
            stringValue.push_back(character);
        }
    }

    void Handle() override {
        auto* assosiate = EntityManager::Instance()->GetEntity(objectID);

        Game::logger->Log("Activity State Change", "%s [%i, %i] from %i to %i\n", GeneralUtils::UTF16ToWTF8(stringValue).c_str(), value1, value2, associate->GetLOT(), assosiate != nullptr ? assosiate->GetLOT() : 0);

        std::vector<Entity*> scriptedActs = EntityManager::Instance()->GetEntitiesByComponent(COMPONENT_TYPE_SHOOTING_GALLERY);
        for (Entity* scriptEntity : scriptedActs) {
            scriptEntity->OnActivityStateChangeRequest(objectID, value1, value2, stringValue);
        }

        associate->OnActivityStateChangeRequest(objectID, value1, value2, stringValue);
    }
};