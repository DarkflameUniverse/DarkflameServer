#pragma once

#include <string>
#include "dCommonVars.h"

struct BehaviorAction {
    std::string actionName;                 //!< The name of the action this behavior performs
    std::string parameterName;              //!< The (optional) name of the parameter this action has
    std::string parameterValueString;       //!< The (optional) string value this behavior action has for a value
    double parameterValueDouble;            //!< The (optional) double value this behavior action has for a value
    std::string callbackID = "";            //!< Unknown what this is used for.  Kept in code as it is in packet captures
    double xPosition;                       //!< The x Position of the head of this strip
    double yPosition;                       //!< The y Position of the head of this strip
    bool isActive;                          //!< Whether or not this action is active
};

enum eStates : BEHAVIORSTATE {
    HOME_STATE = 0,                         //!< The HOME behavior state
    CIRCLE_STATE,                           //!< The CIRCLE behavior state
    SQUARE_STATE,                           //!< The SQUARE behavior state
    DIAMOND_STATE,                          //!< The DIAMOND behavior state
    TRIANGLE_STATE,                         //!< The TRIANGLE behavior state
    STAR_STATE                              //!< The STAR behavior state
};