#pragma once

#include "PropertyBehaviors.h"
#include <string>

struct BehaviorAction {
    std::string behaviorName;
    std::string actionName;
    std::string parameterName;
    std::string parameterValue;
    double parameterValueNumber;
    STRIPID stripID;
    double xPosition;
    double yPosition;
    BEHAVIORSTATE stateID;
    uint32_t behaviorID;
    bool enabled;
    uint32_t uiSortOrder;
    bool isLoot;
    bool isLocked;
    LWOOBJID parentModelObjectID;
    std::string callbackID = "";
};

enum eStates : BEHAVIORSTATE {
    HOME_STATE = 0,
    CIRCLE_STATE,
    SQUARE_STATE,
    DIAMOND_STATE,
    TRIANGLE_STATE,
    STAR_STATE
};