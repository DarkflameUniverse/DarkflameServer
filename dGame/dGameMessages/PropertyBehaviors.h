#pragma once

#include <cstdint>
#include <vector>
#include <map>
#include "dCommonVars.h"
#include "dLogger.h"
#include "Game.h"
#include "GameMessages.h"
#include "Entity.h"
#include "Actions.h"

class PropertyBehaviors {
    public:
        void AddStrip(uint32_t stripID, double xPos, double yPos, uint32_t stateID, uint32_t behaviorID, LWOOBJID modelObjectID, std::string actionName, std::string actionParameter);
        void SerializeStrip(LWOOBJID objectID, const SystemAddress& sysAddr, Entity* modelOwner);
    private:
        std::map<BEHAVIORSTATE, std::map<STRIPID, std::vector<BehaviorAction*>>> states = {
            {eStates::HOME_STATE, {}},
        };
};

/**
*               {eStates::CIRCLE_STATE, {}},
*     {eStates::SQUARE_STATE, {}},
*     {eStates::DIAMOND_STATE, {}},
*     {eStates::TRIANGLE_STATE, {}},
*     {eStates::STAR_STATE, {}}
*    
 */