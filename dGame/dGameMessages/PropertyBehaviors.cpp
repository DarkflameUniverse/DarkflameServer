#include "PropertyBehaviors.h"

// TODO
void PropertyBehaviors::AddStrip(STRIPID stripID, double xPos, double yPos, BEHAVIORSTATE stateID, uint32_t behaviorID, LWOOBJID modelObjectID, std::string actionName, std::string actionParameter) {
    // Get state
    // Check that strip is empty
    // If empty, add a new behavior action
    // then add to strips.
    Game::logger->Log("PropertyBehaviors", "Creating new strip!\n");
    auto strip = states.at(stateID).find(stripID);

    if (strip != states.at(stateID).end()) {
        Game::logger->Log("PropertyBehaviors", "Behavior strip of id %i already exists\n", stripID);
        return;
    }

    BehaviorAction* newBehavior = new BehaviorAction();
    newBehavior->actionName = actionName;
    newBehavior->parameterValue = actionParameter;
    newBehavior->stripID = stripID;
    newBehavior->behaviorID = behaviorID;
    newBehavior->parentModelObjectID = modelObjectID;
    newBehavior->xPosition = xPos;
    newBehavior->yPosition = yPos;

    strip->second.push_back(newBehavior);
}

void PropertyBehaviors::SerializeStrip(LWOOBJID objectID, const SystemAddress& sysAddr, Entity* modelOwner) {
    auto tempState = states.at(0);
    std::vector<BehaviorAction*> newThing;
    tempState.insert(std::make_pair(0, newThing));
    Game::logger->Log("PropertyBehaviors", "Initializing starter block!\n");
    BehaviorAction* starterBlock = new BehaviorAction();
    starterBlock->callbackID = "";
    starterBlock->actionName = "OnEnterProximity";
    starterBlock->xPosition = 103.0;
    starterBlock->yPosition = 82.0;
    starterBlock->parentModelObjectID = objectID;
    starterBlock->stripID = 0;
    starterBlock->behaviorID = 10447;
    starterBlock->stateID = 0;
    starterBlock->parameterValueNumber = 1.0;
    starterBlock->parameterName = "Distance";
    starterBlock->parameterValue = "";
    Game::logger->Log("PropertyBehaviors", "finished initializing starter block!\n");
    tempState.at(0).push_back(starterBlock);
    Game::logger->Log("PropertyBehaviors", "Initializing action1 block!\n");
    BehaviorAction* firstAction = new BehaviorAction();

    firstAction->callbackID = "";
    firstAction->actionName = "FlyUp";
    firstAction->parameterName = "Distance";
    firstAction->parameterValueNumber = 25.0;
    Game::logger->Log("PropertyBehaviors", "finished initializing action1 block!\n");
    tempState.at(0).push_back(firstAction);
    Game::logger->Log("PropertyBehaviors", "Initializing action2 block!\n");
    BehaviorAction* secondAction = new BehaviorAction();

    secondAction->callbackID = "";
    secondAction->actionName = "FlyDown";
    secondAction->parameterName = "Distance";
    secondAction->parameterValueNumber = 25.0;
    Game::logger->Log("PropertyBehaviors", "finished initializing action2 block!\n");
    tempState.at(0).push_back(secondAction);
    states[1] = tempState;

    BehaviorAction* thirdAction = new BehaviorAction();
    thirdAction->callbackID = "";
    thirdAction->actionName = "ChangeStateCircle";
    thirdAction->parameterValueNumber = 0.0;
    thirdAction->parameterValue = "";
    tempState.at(0).push_back(thirdAction);

    states[0] = tempState;
    states[2] = tempState;
    states[3] = tempState;
    states[4] = tempState;
    states[5] = tempState;
    Game::logger->Log("PropertyBehaviors", "Begin serialization!\n");
    // Begin serialization.

    /**
     * for each state
     *      strip id
     *      ui info
     *          x
     *          y
     *      actions
     *          action1
     *          action2
     *          ...
     * behaviorID of strip
     * objectID of strip
     */
    LWOOBJID targetObjectID = LWOOBJID_EMPTY;
    uint32_t behaviorID = 0;
    AMFArrayValue behaviorInfo;
    AMFArrayValue* stateSerialize = new AMFArrayValue();
    for (auto it = states.begin(); it != states.end(); it++) {
        Game::logger->Log("PropertyBehaviors", "Begin serialization of state %i!\n", it->first);
        AMFArrayValue* state = new AMFArrayValue();
        AMFDoubleValue* stateAsDouble = new AMFDoubleValue();
        stateAsDouble->SetDoubleValue(it->first);
        state->InsertValue("id", stateAsDouble);
        AMFArrayValue* strips = new AMFArrayValue();
        for (auto strip = it->second.begin(); strip != it->second.end(); strip++) {
            Game::logger->Log("PropertyBehaviors", "Begin serialization of strip %i!\n", strip->first);
            AMFArrayValue* thisStrip = new AMFArrayValue();
            if (strip == it->second.begin()) {
                AMFDoubleValue* stripID = new AMFDoubleValue();
                stripID->SetDoubleValue(strip->first);
                thisStrip->InsertValue("id", stripID);
                AMFArrayValue* uiArray = new AMFArrayValue();
                AMFDoubleValue* yPosition = new AMFDoubleValue();
                AMFDoubleValue* xPosition = new AMFDoubleValue();
                yPosition->SetDoubleValue(strip->second.at(0)->yPosition);
                xPosition->SetDoubleValue(strip->second.at(0)->xPosition);
                uiArray->InsertValue("y", yPosition);
                uiArray->InsertValue("x", xPosition);
                thisStrip->InsertValue("ui", uiArray);
                targetObjectID = strip->second.at(0)->parentModelObjectID;
                behaviorID = strip->second.at(0)->behaviorID;
            }
            AMFArrayValue* stripSerialize = new AMFArrayValue();
            for (auto behaviorAction : strip->second) {
                Game::logger->Log("PropertyBehaviors", "Begin serialization of action %s!\n", behaviorAction->actionName.c_str());
                AMFArrayValue* thisAction = new AMFArrayValue();
                AMFStringValue* actionName = new AMFStringValue();
                actionName->SetStringValue(behaviorAction->actionName);
                thisAction->InsertValue("Type", actionName);
                if (behaviorAction->parameterValue != "") {
                    AMFStringValue* valueAsString = new AMFStringValue();
                    valueAsString->SetStringValue(behaviorAction->parameterValue);
                    thisAction->InsertValue(behaviorAction->parameterName, valueAsString);
                } else if (behaviorAction->parameterValueNumber != 0.0) {
                    AMFDoubleValue* valueAsDouble = new AMFDoubleValue();
                    valueAsDouble->SetDoubleValue(behaviorAction->parameterValueNumber);
                    thisAction->InsertValue(behaviorAction->parameterName, valueAsDouble);
                }
                stripSerialize->PushBackValue(thisAction);
            }
            thisStrip->InsertValue("actions", stripSerialize);
            strips->PushBackValue(thisStrip);
        }
        state->InsertValue("strips", strips);
        stateSerialize->PushBackValue(state);
    }
    behaviorInfo.InsertValue("states", stateSerialize);
    AMFStringValue* objectidAsString = new AMFStringValue();
    objectidAsString->SetStringValue(std::to_string(targetObjectID));
    behaviorInfo.InsertValue("objectID", objectidAsString);
    AMFStringValue* behaviorIDAsString = new AMFStringValue();
    behaviorIDAsString->SetStringValue(std::to_string(behaviorID));
    behaviorInfo.InsertValue("BehaviorID", behaviorIDAsString);

    GameMessages::SendUIMessageServerToSingleClient(modelOwner, sysAddr, "UpdateBehaviorBlocks", &behaviorInfo);
}