#include "ControlBehaviors.h"
#include "ModelComponent.h"

void ControlBehaviors::DoActions(Entity* modelEntity, const SystemAddress& sysAddr, AMFArrayValue* arguments, std::string command, Entity* modelOwner) {
    if (command == "sendBehaviorListToClient")
        sendBehaviorListToClient(modelEntity, sysAddr, arguments, modelOwner);
    else if (command == "modelTypeChanged")
        modelTypeChanged(modelEntity, sysAddr, arguments, modelOwner);
    else if (command == "toggleExecutionUpdates")
        toggleExecutionUpdates(modelEntity, sysAddr, arguments, modelOwner);
    else if (command == "addStrip")
        addStrip(modelEntity, sysAddr, arguments, modelOwner);
    else if (command == "removeStrip")
        removeStrip(modelEntity, sysAddr, arguments, modelOwner);
    else if (command == "mergeStrips")
        mergeStrips(modelEntity, sysAddr, arguments, modelOwner);
    else if (command == "splitStrip")
        splitStrip(modelEntity, sysAddr, arguments, modelOwner);
    else if (command == "updateStripUI")
        updateStripUI(modelEntity, sysAddr, arguments, modelOwner);
    else if (command == "addAction")
        addAction(modelEntity, sysAddr, arguments, modelOwner);
    else if (command == "migrateActions")
        migrateActions(modelEntity, sysAddr, arguments, modelOwner);
    else if (command == "rearrangeStrip")
        rearrangeStrip(modelEntity, sysAddr, arguments, modelOwner);
    else if (command == "add")
        add(modelEntity, sysAddr, arguments, modelOwner);
    else if (command == "removeActions")
        removeActions(modelEntity, sysAddr, arguments, modelOwner);
    else if (command == "rename")
        rename(modelEntity, sysAddr, arguments, modelOwner);
    else
        Game::logger->Log("ControlBehaviors", "Unknown Message (%s)\n", command.c_str());
}

void ControlBehaviors::sendBehaviorListToClient(Entity* modelEntity, const SystemAddress& sysAddr, AMFArrayValue* arguments, Entity* modelOwner) {
    Game::logger->Log("ControlBehaviors", "sendBehaviorListToClient!\n");
    AMFArrayValue behaviorList;
    // For each behavior, get its info and add it to the list
    AMFArrayValue* behaviors = new AMFArrayValue();
	AMFArrayValue* behaviorInfo = new AMFArrayValue();

	AMFStringValue* amfStringValueForObjectID = new AMFStringValue();
	amfStringValueForObjectID->SetStringValue(std::to_string(modelEntity->GetObjectID()));

	behaviorList.InsertValue("objectID", amfStringValueForObjectID);
	GameMessages::SendUIMessageServerToSingleClient(modelOwner, sysAddr, "UpdateBehaviorList", &behaviorList);
}

void ControlBehaviors::modelTypeChanged(Entity* modelEntity, const SystemAddress& sysAddr, AMFArrayValue* arguments, Entity* modelOwner) {
    Game::logger->Log("ControlBehaviors", "modelTypeChanged!\n");
}

void ControlBehaviors::toggleExecutionUpdates(Entity* modelEntity, const SystemAddress& sysAddr, AMFArrayValue* arguments, Entity* modelOwner) {
    Game::logger->Log("ControlBehaviors", "toggleExecutionUpdates!\n");
    auto modelComponent = modelEntity->GetComponent<ModelComponent>();
    if (!modelComponent) return;

    // modelComponent->AddStrip(0, 0, "OnEnterProximity", "Distance", "", 1.0, "", 103.0, 82.0, 10447, "");
    // modelComponent->AddStrip(1, 0, "SpinNegative", "Degrees", "", 90.0, "", 200.0, 182.0, 10447, "");
    // modelComponent->AddStrip(2, 0, "FlyUp", "Distance", "", 25.0, "", 25.0, 382.0, 10447, "");
    // modelComponent->AddStrip(3, 0, "SpawnStromling", "", "", 0.0, "", 103.0, 82.0, 10447, "");
    // modelComponent->AddStrip(4, 0, "DropArmor", "Amount", "", 5.0, "", 103.0, 82.0, 10447, "");
    // modelComponent->AddStrip(5, 0, "Wait", "Delay", "", 5.0, "", 103.0, 82.0, 10447, "");

    auto states = modelComponent->GetBehaviorActions();

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

            Game::logger->Log("PropertyBehaviors", "Begin serialization of location for strip %i!\n", strip->first);
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

void ControlBehaviors::addStrip(Entity* modelEntity, const SystemAddress& sysAddr, AMFArrayValue* arguments, Entity* modelOwner) {
    Game::logger->Log("ControlBehaviors", "addStrip!\n");
    auto modelComponent = modelEntity->GetComponent<ModelComponent>();
    if (!modelComponent) return;

    AMFArrayValue* strip = (AMFArrayValue*)arguments->FindValue("strip");
    AMFArrayValue* actions = (AMFArrayValue*)strip->FindValue("actions");
    AMFArrayValue* uiArray = (AMFArrayValue*)arguments->FindValue("ui");
    AMFDoubleValue* xPositionValue = (AMFDoubleValue*)uiArray->FindValue("x");
    AMFDoubleValue* yPositionValue = (AMFDoubleValue*)uiArray->FindValue("y");
    double xPosition = xPositionValue->GetDoubleValue();
    double yPosition = yPositionValue->GetDoubleValue();
    AMFDoubleValue* stripIDValue = (AMFDoubleValue*)arguments->FindValue("stripID");
    AMFDoubleValue* stateIDValue = (AMFDoubleValue*)arguments->FindValue("stateID");
    STRIPID stripID = (STRIPID)stripIDValue->GetDoubleValue();
    BEHAVIORSTATE stateID = (BEHAVIORSTATE)stateIDValue->GetDoubleValue();

    std::string type = "";
    std::string valueParameterName = "";
    std::string valueParameterString = "";
    double valueParameterDouble = 0.0;
    auto actionList = actions->GetDenseArray();
    for (AMFValue* action : actionList) {
        AMFArrayValue* actionAsArray = (AMFArrayValue*)action;
        for (auto typeValueMap : actionAsArray->GetAssociativeMap()) {
            if (typeValueMap.first == "Type") {
                type = ((AMFStringValue*)typeValueMap.second)->GetStringValue();
            } else {
                valueParameterName = typeValueMap.first;
                // Message is the only known string parameter
                if (valueParameterName == "Message") {
                    valueParameterString = ((AMFStringValue*)typeValueMap.second)->GetStringValue();
                } else {
                    valueParameterDouble = ((AMFDoubleValue*)typeValueMap.second)->GetDoubleValue();
                }
            }
        }
        modelComponent->AddStrip(stateID, stripID, type, valueParameterName, valueParameterString, valueParameterDouble, "", xPosition, yPosition, 0, "");
        type = "";
        valueParameterName = "";
        valueParameterString = "";
        valueParameterDouble = 0.0;
    }
}

void ControlBehaviors::removeStrip(Entity* modelEntity, const SystemAddress& sysAddr, AMFArrayValue* arguments, Entity* modelOwner) {
    Game::logger->Log("ControlBehaviors", "removeStrip!\n");
}

void ControlBehaviors::mergeStrips(Entity* modelEntity, const SystemAddress& sysAddr, AMFArrayValue* arguments, Entity* modelOwner) {
    Game::logger->Log("ControlBehaviors", "mergeStrips!\n");
}

void ControlBehaviors::splitStrip(Entity* modelEntity, const SystemAddress& sysAddr, AMFArrayValue* arguments, Entity* modelOwner) {
    Game::logger->Log("ControlBehaviors", "splitStrip!\n");
}

void ControlBehaviors::updateStripUI(Entity* modelEntity, const SystemAddress& sysAddr, AMFArrayValue* arguments, Entity* modelOwner) {
    Game::logger->Log("ControlBehaviors", "updateStripUI!\n");
}

void ControlBehaviors::addAction(Entity* modelEntity, const SystemAddress& sysAddr, AMFArrayValue* arguments, Entity* modelOwner) {
    Game::logger->Log("ControlBehaviors", "addAction!\n");
    // auto modelComponent = modelEntity->GetComponent<ModelComponent>();
    // if (!modelComponent) return;

    // AMFArrayValue* actions = (AMFArrayValue*)strip->FindValue("actions");

    // AMFDoubleValue* actionIndexValue = (AMFDoubleValue*)arguments->FindValue("actionIndex");
    // uint32_t actionIndex = (uint32_t)actionIndexValue->GetDoubleValue();

    // AMFDoubleValue* stripIDValue = (AMFDoubleValue*)arguments->FindValue("stripID");
    // STRIPID stripID = (STRIPID)stripIDValue->GetDoubleValue();

    // AMFDoubleValue* stateIDValue = (AMFDoubleValue*)arguments->FindValue("stateID");
    // BEHAVIORSTATE stateID = (BEHAVIORSTATE)stateIDValue->GetDoubleValue();

    // std::string type = "";
    // std::string valueParameterName = "";
    // std::string valueParameterString = "";
    // double valueParameterDouble = 0.0;
    // auto actionList = actions->GetDenseArray();
    // AMFArrayValue* actionAsArray = (AMFArrayValue*)actionList;
    // for (auto typeValueMap : actionAsArray->GetAssociativeMap()) {
    //     if (typeValueMap.first == "Type") {
    //         type = ((AMFStringValue*)typeValueMap.second)->GetStringValue();
    //     } else {
    //         valueParameterName = typeValueMap.first;
    //         // Message is the only known string parameter
    //         if (valueParameterName == "Message") {
    //             valueParameterString = ((AMFStringValue*)typeValueMap.second)->GetStringValue();
    //         } else {
    //             valueParameterDouble = ((AMFDoubleValue*)typeValueMap.second)->GetDoubleValue();
    //         }
    //     }
    // }
}

void ControlBehaviors::migrateActions(Entity* modelEntity, const SystemAddress& sysAddr, AMFArrayValue* arguments, Entity* modelOwner) {
    Game::logger->Log("ControlBehaviors", "migrateActions!\n");
}

void ControlBehaviors::rearrangeStrip(Entity* modelEntity, const SystemAddress& sysAddr, AMFArrayValue* arguments, Entity* modelOwner) {
    Game::logger->Log("ControlBehaviors", "rearrangeStrip!\n");
}

void ControlBehaviors::add(Entity* modelEntity, const SystemAddress& sysAddr, AMFArrayValue* arguments, Entity* modelOwner) {
    Game::logger->Log("ControlBehaviors", "add!\n");
}

void ControlBehaviors::removeActions(Entity* modelEntity, const SystemAddress& sysAddr, AMFArrayValue* arguments, Entity* modelOwner) {
    Game::logger->Log("ControlBehaviors", "removeActions!\n");
}

void ControlBehaviors::rename(Entity* modelEntity, const SystemAddress& sysAddr, AMFArrayValue* arguments, Entity* modelOwner) {
    Game::logger->Log("ControlBehaviors", "rename!\n");
}