#pragma once

#include <string>
#include "dCommonVars.h"

struct BehaviorAction {
    std::string behaviorName;               //!< The name of this behavior
    std::string actionName;                 //!< The name of this action
    std::string parameterName;              //!< The (optional) name of the parameter this action has
    std::string parameterValue;             //!< The (optional) string value this behavior action has for a value
    double parameterValueNumber;            //!< The (optional) double value this behavior action has for a value
    STRIPID stripID;                        //!< The StripID that this action belongs to. (UNUSED i believe)
    double xPosition;                       //!< The x position that this behavior has on the UI.  This should be moved to the ModelBehavior object instead.
    double yPosition;                       //!< The y position that this behavior has on the UI.  This should be moved to the ModelBehavior object instead.
    BEHAVIORSTATE stateID;                  //!< The stateID that this action belongs to. (UNUSED i believe)
    uint32_t behaviorID;                    //!< The behavior ID this action belongs to. (UNUSED and should be moved to ModelBehavior object)
    bool enabled;                           //!< ??? (UNUSED)
    uint32_t uiSortOrder;                   //!< ??? (UNUSED)
    bool isLoot;                            //!< UNUSED
    bool isLocked;                          //!< UNUSED
    LWOOBJID parentModelObjectID;           //!< Should be removed and moved somewhere else.
    std::string callbackID = "";            //!< Unknown what this is used for.  Kept in code as it is in packet captures.
};

enum eStates : BEHAVIORSTATE {
    HOME_STATE = 0,                         //!< The HOME behavior state
    CIRCLE_STATE,                           //!< The CIRCLE behavior state
    SQUARE_STATE,                           //!< The SQUARE behavior state
    DIAMOND_STATE,                          //!< The DIAMOND behavior state
    TRIANGLE_STATE,                         //!< The TRIANGLE behavior state
    STAR_STATE                              //!< The STAR behavior state
};