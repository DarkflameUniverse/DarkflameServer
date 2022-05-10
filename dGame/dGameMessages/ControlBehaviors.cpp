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
    else if (command == "sendBehaviorBlocksToClient")
        sendBehaviorBlocksToClient(modelEntity, sysAddr, arguments, modelOwner);
    else if (command == "moveToInventory")
        moveToInventory(modelEntity, sysAddr, arguments, modelOwner);
    else if (command == "updateAction")
        updateAction(modelEntity, sysAddr, arguments, modelOwner);
    else
        Game::logger->Log("ControlBehaviors", "Unknown Message (%s)\n", command.c_str());
}

void ControlBehaviors::sendBehaviorListToClient(Entity* modelEntity, const SystemAddress& sysAddr, AMFArrayValue* arguments, Entity* modelOwner) {
    Game::logger->Log("ControlBehaviors", "sendBehaviorListToClient!\n");
    auto modelComponent = modelEntity->GetComponent<ModelComponent>();
    if (!modelComponent) return;

    auto behaviorList = modelComponent->GetBehaviors();

    AMFArrayValue behaviorsToSerialize;
    // For each behavior, get its info and add it to the list
    AMFArrayValue* behaviors = new AMFArrayValue();
    for (auto behavior : behaviorList) {
        AMFArrayValue* behaviorInfo = new AMFArrayValue();

        AMFStringValue* id = new AMFStringValue();
        id->SetStringValue(std::to_string(behavior->GetBehaviorID()));

        behaviorInfo->InsertValue("id", id);

        AMFTrueValue* isTrue = new AMFTrueValue();
        AMFFalseValue* isFalse = new AMFFalseValue();
        if (behavior->GetIsLocked()) {
            behaviorInfo->InsertValue("isLocked", isTrue);
        } else {
            behaviorInfo->InsertValue("isLocked", isFalse);
        }
        if (behavior->GetIsLoot()) {
            behaviorInfo->InsertValue("isLoot", isTrue);
        } else {
            behaviorInfo->InsertValue("isLoot", isFalse);
        }

        AMFStringValue* name = new AMFStringValue();
        name->SetStringValue(behavior->GetName());
        behaviorInfo->InsertValue("name", name);

        behaviors->PushBackValue(behaviorInfo);
    }

    behaviorsToSerialize.InsertValue("behaviors", behaviors);

	AMFStringValue* amfStringValueForObjectID = new AMFStringValue();
	amfStringValueForObjectID->SetStringValue(std::to_string(modelEntity->GetObjectID()));

	behaviorsToSerialize.InsertValue("objectID", amfStringValueForObjectID);
	GameMessages::SendUIMessageServerToSingleClient(modelOwner, sysAddr, "UpdateBehaviorList", &behaviorsToSerialize);
}

void ControlBehaviors::modelTypeChanged(Entity* modelEntity, const SystemAddress& sysAddr, AMFArrayValue* arguments, Entity* modelOwner) {
    Game::logger->Log("ControlBehaviors", "modelTypeChanged!\n");
    auto modelComponent = modelEntity->GetComponent<ModelComponent>();
    if (!modelComponent) return;
    AMFDoubleValue* modelTypeValue = (AMFDoubleValue*)arguments->FindValue("ModelType");
    uint32_t modelType = (uint32_t)modelTypeValue->GetDoubleValue();

    modelComponent->SetModelType(modelType);
    delete modelTypeValue;
    modelTypeValue = nullptr;
}

void ControlBehaviors::toggleExecutionUpdates(Entity* modelEntity, const SystemAddress& sysAddr, AMFArrayValue* arguments, Entity* modelOwner) {
    Game::logger->Log("ControlBehaviors", "toggleExecutionUpdates!\n");
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

    AMFValue* behaviorIDValue = arguments->FindValue("BehaviorID");
    uint32_t behaviorID = -1;
    if (behaviorIDValue->GetValueType() != AMFValueType::AMFUndefined) {
        behaviorID = std::stoi(((AMFStringValue*)behaviorIDValue)->GetStringValue());
    }
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
        modelComponent->AddStrip(stateID, stripID, type, behaviorID, valueParameterName, valueParameterString, valueParameterDouble, "", xPosition, yPosition, "");
        type = "";
        valueParameterName = "";
        valueParameterString = "";
        valueParameterDouble = 0.0;
    }
    sendBehaviorListToClient(modelEntity, sysAddr, arguments, modelOwner);

    AMFArrayValue args;

    AMFStringValue* behaviorIDString = new AMFStringValue();
    behaviorIDString->SetStringValue(std::to_string(behaviorID));
    args.InsertValue("behaviorID", behaviorIDString);
    AMFStringValue* objectidasstring = new AMFStringValue();
    objectidasstring->SetStringValue(std::to_string(modelEntity->GetObjectID()));
    args.InsertValue("objectID", objectidasstring);

    GameMessages::SendUIMessageServerToSingleClient(modelOwner, modelOwner->GetParentUser()->GetSystemAddress(), "UpdateBehaviorID", &args);
}

void ControlBehaviors::removeStrip(Entity* modelEntity, const SystemAddress& sysAddr, AMFArrayValue* arguments, Entity* modelOwner) {
    Game::logger->Log("ControlBehaviors", "removeStrip!\n");
    auto modelComponent = modelEntity->GetComponent<ModelComponent>();
    if (!modelComponent) return;

    AMFDoubleValue* stripIDValue = (AMFDoubleValue*)arguments->FindValue("stripID");
    STRIPID stripID = stripIDValue->GetDoubleValue();
    AMFDoubleValue* stateIDValue = (AMFDoubleValue*)arguments->FindValue("stateID");
    BEHAVIORSTATE stateID = stateIDValue->GetDoubleValue();

    AMFValue* behaviorIDValue = arguments->FindValue("BehaviorID");
    uint32_t behaviorID = -1;
    if (behaviorIDValue->GetValueType() != AMFValueType::AMFUndefined) {
        behaviorID = std::stoi(((AMFStringValue*)behaviorIDValue)->GetStringValue());
    }

    modelComponent->RemoveStrip(stateID, stripID, behaviorID);
}

void ControlBehaviors::mergeStrips(Entity* modelEntity, const SystemAddress& sysAddr, AMFArrayValue* arguments, Entity* modelOwner) {
    Game::logger->Log("ControlBehaviors", "mergeStrips!\n");
    auto modelComponent = modelEntity->GetComponent<ModelComponent>();
    if (!modelComponent) return;

    AMFDoubleValue* srcStripIDValue = (AMFDoubleValue*)arguments->FindValue("srcStripID");
    STRIPID srcStripID = (STRIPID)srcStripIDValue->GetDoubleValue();

    AMFDoubleValue* dstStateIDValue = (AMFDoubleValue*)arguments->FindValue("dstStateID");
    BEHAVIORSTATE dstStateID = (BEHAVIORSTATE)dstStateIDValue->GetDoubleValue();

    AMFDoubleValue* srcStateIDValue = (AMFDoubleValue*)arguments->FindValue("srcStateID");
    BEHAVIORSTATE srcStateID = (BEHAVIORSTATE)srcStateIDValue->GetDoubleValue();

    AMFDoubleValue* dstActionIndexValue = (AMFDoubleValue*)arguments->FindValue("dstActionIndex");
    uint32_t dstActionIndex = (uint32_t)dstActionIndexValue->GetDoubleValue();

    AMFDoubleValue* dstStripIDValue = (AMFDoubleValue*)arguments->FindValue("dstStripID");
    STRIPID dstStripID = (STRIPID)dstStripIDValue->GetDoubleValue();

    AMFValue* behaviorIDValue = arguments->FindValue("BehaviorID");
    uint32_t behaviorID = -1;
    if (behaviorIDValue->GetValueType() != AMFValueType::AMFUndefined) {
        behaviorID = std::stoi(((AMFStringValue*)behaviorIDValue)->GetStringValue());
    }

    modelComponent->MergeStrips(srcStripID, dstStripID, srcStateID, dstStateID, behaviorID, dstActionIndex);
}

void ControlBehaviors::splitStrip(Entity* modelEntity, const SystemAddress& sysAddr, AMFArrayValue* arguments, Entity* modelOwner) {
    Game::logger->Log("ControlBehaviors", "splitStrip!\n");
    auto modelComponent = modelEntity->GetComponent<ModelComponent>();
    if (!modelComponent) return;

    AMFDoubleValue* srcActionIndexValue = (AMFDoubleValue*)arguments->FindValue("srcActionIndex");
    uint32_t srcActionIndex = (uint32_t)srcActionIndexValue->GetDoubleValue();

    AMFDoubleValue* srcStripIDValue = (AMFDoubleValue*)arguments->FindValue("srcStripID");
    STRIPID srcStripID = (STRIPID)srcStripIDValue->GetDoubleValue();

    AMFDoubleValue* srcStateIDValue = (AMFDoubleValue*)arguments->FindValue("srcStateID");
    BEHAVIORSTATE srcStateID = (BEHAVIORSTATE)srcStateIDValue->GetDoubleValue();

    AMFDoubleValue* dstStripIDValue = (AMFDoubleValue*)arguments->FindValue("dstStripID");
    STRIPID dstStripID = (STRIPID)dstStripIDValue->GetDoubleValue();

    AMFDoubleValue* dstStateIDValue = (AMFDoubleValue*)arguments->FindValue("dstStateID");
    BEHAVIORSTATE dstStateID = (BEHAVIORSTATE)dstStateIDValue->GetDoubleValue();

    AMFArrayValue* dstStripUIArray = (AMFArrayValue*)arguments->FindValue("dstStripUI");

    AMFDoubleValue* xPositionValue = (AMFDoubleValue*)dstStripUIArray->FindValue("x");
    AMFDoubleValue* yPositionValue = (AMFDoubleValue*)dstStripUIArray->FindValue("y");
    double yPosition = yPositionValue->GetDoubleValue();
    double xPosition = xPositionValue->GetDoubleValue();

    AMFValue* behaviorIDValue = arguments->FindValue("BehaviorID");
    uint32_t behaviorID = -1;
    if (behaviorIDValue->GetValueType() != AMFValueType::AMFUndefined) {
        behaviorID = std::stoi(((AMFStringValue*)behaviorIDValue)->GetStringValue());
    }

    modelComponent->SplitStrip(srcActionIndex, srcStripID, srcStateID, dstStripID, dstStateID, behaviorID, yPosition, xPosition);
}

void ControlBehaviors::updateStripUI(Entity* modelEntity, const SystemAddress& sysAddr, AMFArrayValue* arguments, Entity* modelOwner) {
    Game::logger->Log("ControlBehaviors", "updateStripUI!\n");
    auto modelComponent = modelEntity->GetComponent<ModelComponent>();
    if (!modelComponent) return;

    AMFArrayValue* uiArray = (AMFArrayValue*)arguments->FindValue("ui");
    AMFDoubleValue* xPositionValue = (AMFDoubleValue*)uiArray->FindValue("x");
    AMFDoubleValue* yPositionValue = (AMFDoubleValue*)uiArray->FindValue("y");
    double yPosition = yPositionValue->GetDoubleValue();
    double xPosition = xPositionValue->GetDoubleValue();

    AMFDoubleValue* stripIDValue = (AMFDoubleValue*)arguments->FindValue("stripID");
    double stripID = stripIDValue->GetDoubleValue();
    AMFDoubleValue* stateIDValue = (AMFDoubleValue*)arguments->FindValue("stateID");
    double stateID = stateIDValue->GetDoubleValue();

    AMFValue* behaviorIDValue = arguments->FindValue("BehaviorID");
    uint32_t behaviorID = -1;
    if (behaviorIDValue->GetValueType() != AMFValueType::AMFUndefined) {
        behaviorID = std::stoi(((AMFStringValue*)behaviorIDValue)->GetStringValue());
    }

    modelComponent->UpdateUIOfStrip(stateID, stripID, xPosition, yPosition, behaviorID);
}

void ControlBehaviors::addAction(Entity* modelEntity, const SystemAddress& sysAddr, AMFArrayValue* arguments, Entity* modelOwner) {
    Game::logger->Log("ControlBehaviors", "addAction!\n");
    auto modelComponent = modelEntity->GetComponent<ModelComponent>();
    if (!modelComponent) return;

    AMFDoubleValue* actionIndexValue = (AMFDoubleValue*)arguments->FindValue("actionIndex");
    uint32_t actionIndex = (uint32_t)actionIndexValue->GetDoubleValue();

    AMFDoubleValue* stripIDValue = (AMFDoubleValue*)arguments->FindValue("stripID");
    STRIPID stripID = (STRIPID)stripIDValue->GetDoubleValue();

    AMFDoubleValue* stateIDValue = (AMFDoubleValue*)arguments->FindValue("stateID");
    BEHAVIORSTATE stateID = (BEHAVIORSTATE)stateIDValue->GetDoubleValue();

    std::string type = "";
    std::string valueParameterName = "";
    std::string valueParameterString = "";
    double valueParameterDouble = 0.0;
    AMFArrayValue* actionAsArray = (AMFArrayValue*)arguments->FindValue("action");
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

    AMFValue* behaviorIDValue = arguments->FindValue("BehaviorID");
    uint32_t behaviorID = -1;
    if (behaviorIDValue->GetValueType() != AMFValueType::AMFUndefined) {
        behaviorID = std::stoi(((AMFStringValue*)behaviorIDValue)->GetStringValue());
    }

    modelComponent->AddAction(stateID, stripID, type, valueParameterName, valueParameterString, valueParameterDouble, "", actionIndex, behaviorID);
}

void ControlBehaviors::migrateActions(Entity* modelEntity, const SystemAddress& sysAddr, AMFArrayValue* arguments, Entity* modelOwner) {
    Game::logger->Log("ControlBehaviors", "migrateActions!\n");
    auto modelComponent = modelEntity->GetComponent<ModelComponent>();
    if (!modelComponent) return;
    
    AMFDoubleValue* srcActionIndexValue = (AMFDoubleValue*)arguments->FindValue("srcActionIndex");
    uint32_t srcActionIndex = (uint32_t)srcActionIndexValue->GetDoubleValue();

    AMFDoubleValue* srcStripIDValue = (AMFDoubleValue*)arguments->FindValue("srcStripID");
    STRIPID srcStripID = (STRIPID)srcStripIDValue->GetDoubleValue();

    AMFDoubleValue* srcStateIDValue = (AMFDoubleValue*)arguments->FindValue("srcStateID");
    BEHAVIORSTATE srcStateID = (BEHAVIORSTATE)srcStateIDValue->GetDoubleValue();

    AMFDoubleValue* dstActionIndexValue = (AMFDoubleValue*)arguments->FindValue("dstActionIndex");
    uint32_t dstActionIndex = (uint32_t)dstActionIndexValue->GetDoubleValue();

    AMFDoubleValue* dstStripIDValue = (AMFDoubleValue*)arguments->FindValue("dstStripID");
    STRIPID dstStripID = (STRIPID)dstStripIDValue->GetDoubleValue();

    AMFDoubleValue* dstStateIDValue = (AMFDoubleValue*)arguments->FindValue("dstStateID");
    BEHAVIORSTATE dstStateID = (BEHAVIORSTATE)dstStateIDValue->GetDoubleValue();    

    AMFValue* behaviorIDValue = arguments->FindValue("BehaviorID");
    uint32_t behaviorID = -1;
    if (behaviorIDValue->GetValueType() != AMFValueType::AMFUndefined) {
        behaviorID = std::stoi(((AMFStringValue*)behaviorIDValue)->GetStringValue());
    }

    modelComponent->MigrateActions(srcActionIndex, srcStripID, srcStateID, dstActionIndex, dstStripID, dstStateID, behaviorID);
}

void ControlBehaviors::rearrangeStrip(Entity* modelEntity, const SystemAddress& sysAddr, AMFArrayValue* arguments, Entity* modelOwner) {
    Game::logger->Log("ControlBehaviors", "rearrangeStrip!\n");
    auto modelComponent = modelEntity->GetComponent<ModelComponent>();
    if (!modelComponent) return;
    /**
     * key `srcActionIndex`
     * key `stripID`
     * key `BehaviorID`
     * key `dstActionIndex`
     * key `stateID`
     */
    AMFDoubleValue* srcActionIndexValue = (AMFDoubleValue*)arguments->FindValue("srcActionIndex");
    uint32_t srcActionIndex = (uint32_t)srcActionIndexValue->GetDoubleValue();
    AMFDoubleValue* stripIDValue = (AMFDoubleValue*)arguments->FindValue("stripID");
    uint32_t stripID = (uint32_t)stripIDValue->GetDoubleValue();

    AMFValue* behaviorIDValue = arguments->FindValue("BehaviorID");
    uint32_t behaviorID = -1;
    if (behaviorIDValue->GetValueType() != AMFValueType::AMFUndefined) {
        behaviorID = std::stoi(((AMFStringValue*)behaviorIDValue)->GetStringValue());
    }

    AMFDoubleValue* dstActionIndexValue = (AMFDoubleValue*)arguments->FindValue("dstActionIndex");
    uint32_t dstActionIndex = (uint32_t)dstActionIndexValue->GetDoubleValue();
    AMFDoubleValue* stateIDValue = (AMFDoubleValue*)arguments->FindValue("stateID");
    uint32_t stateID = (uint32_t)stateIDValue->GetDoubleValue();

    modelComponent->RearrangeStrip(stateID, stripID, srcActionIndex, dstActionIndex, behaviorID);
}

void ControlBehaviors::add(Entity* modelEntity, const SystemAddress& sysAddr, AMFArrayValue* arguments, Entity* modelOwner) {
    Game::logger->Log("ControlBehaviors", "add!\n");
    auto modelComponent = modelEntity->GetComponent<ModelComponent>();
    if (!modelComponent) return;

    AMFValue* behaviorIDValue = arguments->FindValue("BehaviorID");
    uint32_t behaviorID = -1;
    if (behaviorIDValue->GetValueType() != AMFValueType::AMFUndefined) {
        behaviorID = std::stoi(((AMFStringValue*)behaviorIDValue)->GetStringValue());
    }
    uint32_t behaviorIndex = 0;
    AMFValue* behaviorIndexValue = arguments->FindValue("BehaviorIndex");

    if (behaviorIndexValue) {
        AMFDoubleValue* behaviorIndexValueConverted = (AMFDoubleValue*)behaviorIndexValue;
        behaviorIndex = (uint32_t)behaviorIndexValueConverted->GetDoubleValue();
    }

    modelComponent->AddBehavior(behaviorID, behaviorIndex);
    sendBehaviorListToClient(modelEntity, sysAddr, arguments, modelOwner);
}

void ControlBehaviors::removeActions(Entity* modelEntity, const SystemAddress& sysAddr, AMFArrayValue* arguments, Entity* modelOwner) {
    Game::logger->Log("ControlBehaviors", "removeActions!\n");
    auto modelComponent = modelEntity->GetComponent<ModelComponent>();
    if (!modelComponent) return;

    AMFValue* behaviorIDValue = arguments->FindValue("BehaviorID");
    uint32_t behaviorID = -1;
    if (behaviorIDValue->GetValueType() != AMFValueType::AMFUndefined) {
        behaviorID = std::stoi(((AMFStringValue*)behaviorIDValue)->GetStringValue());
    }
    AMFDoubleValue* actionIndexValue = (AMFDoubleValue*)arguments->FindValue("actionIndex");
    uint32_t actionIndex = (uint32_t)actionIndexValue->GetDoubleValue();
    AMFDoubleValue* stripIDValue = (AMFDoubleValue*)arguments->FindValue("stripID");
    STRIPID stripID = (uint32_t)stripIDValue->GetDoubleValue();
    AMFDoubleValue* stateIDValue = (AMFDoubleValue*)arguments->FindValue("stateID");
    BEHAVIORSTATE stateID = (uint32_t)stateIDValue->GetDoubleValue();

    modelComponent->RemoveAction(stateID, stripID, actionIndex, behaviorID);
}

void ControlBehaviors::rename(Entity* modelEntity, const SystemAddress& sysAddr, AMFArrayValue* arguments, Entity* modelOwner) {
    Game::logger->Log("ControlBehaviors", "rename!\n");
    auto modelComponent = modelEntity->GetComponent<ModelComponent>();
    if (!modelComponent) return;

    AMFValue* behaviorIDValue = arguments->FindValue("BehaviorID");
    uint32_t behaviorID = -1;
    if (behaviorIDValue->GetValueType() != AMFValueType::AMFUndefined) {
        behaviorID = std::stoi(((AMFStringValue*)behaviorIDValue)->GetStringValue());
    }
    AMFStringValue* nameAsValue = (AMFStringValue*)arguments->FindValue("Name");
    auto name = nameAsValue->GetStringValue();

    modelComponent->Rename(behaviorID, name);

    sendBehaviorListToClient(modelEntity, sysAddr, arguments, modelOwner);
}

void ControlBehaviors::sendBehaviorBlocksToClient(Entity* modelEntity, const SystemAddress& sysAddr, AMFArrayValue* arguments, Entity* modelOwner) {
    Game::logger->Log("ControlBehaviors", "sendBehaviorBlocksToClient!\n");
    auto modelComponent = modelEntity->GetComponent<ModelComponent>();
    if (!modelComponent) return;

    AMFValue* behaviorIDValue = arguments->FindValue("BehaviorID");
    uint32_t behaviorID = -1;
    if (behaviorIDValue->GetValueType() != AMFValueType::AMFUndefined) {
        behaviorID = std::stoi(((AMFStringValue*)behaviorIDValue)->GetStringValue());
    }

    auto modelBehavior = modelComponent->FindBehavior(behaviorID);

    if (!modelBehavior) return;

    auto states = modelBehavior->GetBehaviorActions();

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
    behaviorID = 0;
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

void ControlBehaviors::updateAction(Entity* modelEntity, const SystemAddress& sysAddr, AMFArrayValue* arguments, Entity* modelOwner) {
    Game::logger->Log("ControlBehaviors", "updateAction!\n");
    auto modelComponent = modelEntity->GetComponent<ModelComponent>();
    if (!modelComponent) return;

    std::string type = "";
    std::string valueParameterName = "";
    std::string valueParameterString = "";
    double valueParameterDouble = 0.0;
    AMFArrayValue* actionAsArray = (AMFArrayValue*)arguments->FindValue("action");
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

    AMFValue* behaviorIDValue = arguments->FindValue("BehaviorID");
    uint32_t behaviorID = -1;
    if (behaviorIDValue->GetValueType() != AMFValueType::AMFUndefined) {
        behaviorID = std::stoi(((AMFStringValue*)behaviorIDValue)->GetStringValue());
    }

    AMFDoubleValue* actionIndexValue = (AMFDoubleValue*)arguments->FindValue("actionIndex");
    uint32_t actionIndex = (uint32_t)actionIndexValue->GetDoubleValue();

    AMFDoubleValue* stripIDValue = (AMFDoubleValue*)arguments->FindValue("stripID");
    STRIPID stripID = (STRIPID)stripIDValue->GetDoubleValue();

    AMFDoubleValue* stateIDValue = (AMFDoubleValue*)arguments->FindValue("stateID");
    BEHAVIORSTATE stateID = (BEHAVIORSTATE)stateIDValue->GetDoubleValue();

    modelComponent->UpdateAction(stateID, stripID, type, valueParameterName, valueParameterString, valueParameterDouble, "", actionIndex, behaviorID);
}

// TODO This doesn't save behaviors to the inventory at the moment, just removes them from the list of behaviors.
void ControlBehaviors::moveToInventory(Entity* modelEntity, const SystemAddress& sysAddr, AMFArrayValue* arguments, Entity* modelOwner) {
    Game::logger->Log("ControlBehaviors", "moveToInventory!\n");
    
    AMFArrayValue args;

    AMFFalseValue* stateToPop = new AMFFalseValue();
    args.InsertValue("visible", stateToPop);

    GameMessages::SendUIMessageServerToSingleClient(modelOwner, modelOwner->GetParentUser()->GetSystemAddress(), "ToggleBehaviorEditor", &args);

    auto modelComponent = modelEntity->GetComponent<ModelComponent>();
    if (!modelComponent) return;

    AMFValue* behaviorIDValue = arguments->FindValue("BehaviorID");
    uint32_t behaviorID = -1;
    if (behaviorIDValue->GetValueType() != AMFValueType::AMFUndefined) {
        behaviorID = std::stoi(((AMFStringValue*)behaviorIDValue)->GetStringValue());
    }
    AMFDoubleValue* behaviorIndexValue = (AMFDoubleValue*)arguments->FindValue("BehaviorIndex");
    uint32_t behaviorIndex = (uint32_t)behaviorIndexValue->GetDoubleValue();

    modelComponent->MoveBehaviorToInventory(behaviorID, behaviorIndex);

    sendBehaviorListToClient(modelEntity, sysAddr, arguments, modelOwner);
}
