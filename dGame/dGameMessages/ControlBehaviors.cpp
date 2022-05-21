#include "ControlBehaviors.h"
#include "SimplePhysicsComponent.h"
#include "MovementAIComponent.h"
#include "PropertyManagementComponent.h"
#include "../../dWorldServer/ObjectIDManager.h"

ControlBehaviors::~ControlBehaviors() {
    delete this->arguments;
    this->arguments = nullptr;
}

void ControlBehaviors::DoActions(Entity* modelEntity, const SystemAddress& sysAddr, AMFArrayValue* arguments, std::string command, Entity* modelOwner) {
    if (!modelEntity || !modelOwner || !arguments) return;

    this->arguments = arguments;

    modelComponent = modelEntity->GetComponent<ModelComponent>();

    this->modelOwner = modelOwner;

    this->sysAddr = sysAddr;

    if (!modelComponent || !modelOwner) return;

    if (command == "sendBehaviorListToClient")
        SendBehaviorListToClient();
    else if (command == "modelTypeChanged")
        ModelTypeChanged();
    else if (command == "toggleExecutionUpdates")
        ToggleExecutionUpdates();
    else if (command == "addStrip")
        AddStrip();
    else if (command == "removeStrip")
        RemoveStrip();
    else if (command == "mergeStrips")
        MergeStrips();
    else if (command == "splitStrip")
        SplitStrip();
    else if (command == "updateStripUI")
        UpdateStripUI();
    else if (command == "addAction")
        AddAction();
    else if (command == "migrateActions")
        MigrateActions();
    else if (command == "rearrangeStrip")
        RearrangeStrip();
    else if (command == "add")
        Add();
    else if (command == "removeActions")
        RemoveActions();
    else if (command == "rename")
        Rename();
    else if (command == "sendBehaviorBlocksToClient")
        SendBehaviorBlocksToClient();
    else if (command == "moveToInventory")
        MoveToInventory();
    else if (command == "updateAction")
        UpdateAction();
    else
        Game::logger->Log("ControlBehaviors", "Unknown Message (%s)\n", command.c_str());
}

void ControlBehaviors::SendBehaviorListToClient() {
    Game::logger->Log("ControlBehaviors", "sendBehaviorListToClient!\n");

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

        if (behavior->GetIsLocked()) behaviorInfo->InsertValue("isLocked", isTrue);
         else behaviorInfo->InsertValue("isLocked", isFalse);

        if (behavior->GetIsLoot()) behaviorInfo->InsertValue("isLoot", isTrue);
        else behaviorInfo->InsertValue("isLoot", isFalse);

        AMFStringValue* name = new AMFStringValue();
        name->SetStringValue(behavior->GetName());
        behaviorInfo->InsertValue("name", name);

        behaviors->PushBackValue(behaviorInfo);
    }

    behaviorsToSerialize.InsertValue("behaviors", behaviors);

	AMFStringValue* amfStringValueForObjectID = new AMFStringValue();
	amfStringValueForObjectID->SetStringValue(std::to_string(modelComponent->GetParent()->GetObjectID()));

	behaviorsToSerialize.InsertValue("objectID", amfStringValueForObjectID);
	GameMessages::SendUIMessageServerToSingleClient(modelOwner, sysAddr, "UpdateBehaviorList", &behaviorsToSerialize);
}

void ControlBehaviors::ModelTypeChanged() {
    Game::logger->Log("ControlBehaviors", "modelTypeChanged!\n");

    AMFDoubleValue* modelTypeValue = (AMFDoubleValue*)arguments->FindValue("ModelType");
    uint32_t modelType = (uint32_t)modelTypeValue->GetDoubleValue();

    modelComponent->SetModelType(modelType);
    delete modelTypeValue;
    modelTypeValue = nullptr;
}

void ControlBehaviors::ToggleExecutionUpdates() {
    Game::logger->Log("ControlBehaviors", "toggleExecutionUpdates!\n");
}

void ControlBehaviors::AddStrip() {

    Game::logger->Log("ControlBehaviors", "addStrip!\n");

    AMFArrayValue* strip = (AMFArrayValue*)arguments->FindValue("strip");

    AMFArrayValue* actions = (AMFArrayValue*)strip->FindValue("actions");

    AMFArrayValue* uiArray = (AMFArrayValue*)arguments->FindValue("ui");

    AMFDoubleValue* xPositionValue = (AMFDoubleValue*)uiArray->FindValue("x");
    double xPosition = xPositionValue->GetDoubleValue();

    AMFDoubleValue* yPositionValue = (AMFDoubleValue*)uiArray->FindValue("y");
    double yPosition = yPositionValue->GetDoubleValue();

    STRIPID stripID = GetStripIDFromArgument();

    BEHAVIORSTATE stateID = GetBehaviorStateFromArgument();

    uint32_t behaviorID = GetBehaviorIDFromArgument();

    delete uiArray;
    uiArray = nullptr;
    delete xPositionValue;
    xPositionValue = nullptr;
    delete yPositionValue;
    yPositionValue = nullptr;

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
        modelComponent->AddStrip(stateID, stripID, type, behaviorID, valueParameterName, valueParameterString, valueParameterDouble, "", xPosition, yPosition);
        type = "";
        valueParameterName = "";
        valueParameterString = "";
        valueParameterDouble = 0.0;
    }
    if (behaviorID == -1) {
        ObjectIDManager::Instance()->RequestPersistentID([this, &behaviorID](uint32_t persistentId) {
            auto behavior = this->modelComponent->FindBehavior(behaviorID);
            behaviorID = persistentId;
            behavior->SetBehaviorID(persistentId);

            // This updates the behavior ID of the behavior should this be a new behavior
            AMFArrayValue args;

            AMFStringValue* behaviorIDString = new AMFStringValue();
            behaviorIDString->SetStringValue(std::to_string(behaviorID));
            args.InsertValue("behaviorID", behaviorIDString);

            AMFStringValue* objectIDAsString = new AMFStringValue();
            objectIDAsString->SetStringValue(std::to_string(modelComponent->GetParent()->GetObjectID()));
            args.InsertValue("objectID", objectIDAsString);

            GameMessages::SendUIMessageServerToSingleClient(modelOwner, sysAddr, "UpdateBehaviorID", &args);
            delete behaviorIDString;
            behaviorIDString = nullptr;
            delete objectIDAsString;
            objectIDAsString = nullptr;
            SendBehaviorListToClient();
            delete this;
        });
    }
}

void ControlBehaviors::RemoveStrip() {
    Game::logger->Log("ControlBehaviors", "removeStrip!\n");

    STRIPID stripID = GetStripIDFromArgument();

    BEHAVIORSTATE stateID = GetBehaviorStateFromArgument();

    uint32_t behaviorID = GetBehaviorIDFromArgument();

    modelComponent->RemoveStrip(stateID, stripID, behaviorID);
}

void ControlBehaviors::MergeStrips() {
    Game::logger->Log("ControlBehaviors", "mergeStrips!\n");

    STRIPID srcStripID = GetStripIDFromArgument("srcStripID");

    BEHAVIORSTATE dstStateID = GetBehaviorStateFromArgument("dstStateID");

    BEHAVIORSTATE srcStateID = GetBehaviorStateFromArgument("srcStateID");

    AMFDoubleValue* dstActionIndexValue = (AMFDoubleValue*)arguments->FindValue("dstActionIndex");
    uint32_t dstActionIndex = (uint32_t)dstActionIndexValue->GetDoubleValue();

    STRIPID dstStripID = GetStripIDFromArgument("dstStripID");

    uint32_t behaviorID = GetBehaviorIDFromArgument();

    modelComponent->MergeStrips(srcStripID, dstStripID, srcStateID, dstStateID, behaviorID, dstActionIndex);
}

void ControlBehaviors::SplitStrip() {
    Game::logger->Log("ControlBehaviors", "splitStrip!\n");

    AMFDoubleValue* srcActionIndexValue = (AMFDoubleValue*)arguments->FindValue("srcActionIndex");
    uint32_t srcActionIndex = (uint32_t)srcActionIndexValue->GetDoubleValue();

    STRIPID srcStripID = GetStripIDFromArgument("srcStripID");

    BEHAVIORSTATE srcStateID = GetBehaviorStateFromArgument("srcStateID");

    STRIPID dstStripID = GetStripIDFromArgument("dstStripID");

    BEHAVIORSTATE dstStateID = GetBehaviorStateFromArgument("dstStateID");

    AMFArrayValue* dstStripUIArray = (AMFArrayValue*)arguments->FindValue("dstStripUI");

    AMFDoubleValue* xPositionValue = (AMFDoubleValue*)dstStripUIArray->FindValue("x");
    AMFDoubleValue* yPositionValue = (AMFDoubleValue*)dstStripUIArray->FindValue("y");
    // x and y position 15 are just where the game puts the strip by default if none is given.
    double yPosition = yPositionValue != nullptr ? yPositionValue->GetDoubleValue() : 15;
    double xPosition = xPositionValue != nullptr ? xPositionValue->GetDoubleValue() : 15;

    uint32_t behaviorID = GetBehaviorIDFromArgument();

    modelComponent->SplitStrip(srcActionIndex, srcStripID, srcStateID, dstStripID, dstStateID, behaviorID, yPosition, xPosition);
}

void ControlBehaviors::UpdateStripUI() {
    Game::logger->Log("ControlBehaviors", "updateStripUI!\n");

    AMFArrayValue* uiArray = (AMFArrayValue*)arguments->FindValue("ui");

    AMFDoubleValue* xPositionValue = (AMFDoubleValue*)uiArray->FindValue("x");
    AMFDoubleValue* yPositionValue = (AMFDoubleValue*)uiArray->FindValue("y");
    double yPosition = yPositionValue->GetDoubleValue();
    double xPosition = xPositionValue->GetDoubleValue();

    STRIPID stripID = GetStripIDFromArgument();

    BEHAVIORSTATE stateID = GetBehaviorStateFromArgument();

    uint32_t behaviorID = GetBehaviorIDFromArgument();

    modelComponent->UpdateUIOfStrip(stateID, stripID, xPosition, yPosition, behaviorID);
}

void ControlBehaviors::AddAction() {
    Game::logger->Log("ControlBehaviors", "addAction!\n");

    AMFDoubleValue* actionIndexValue = (AMFDoubleValue*)arguments->FindValue("actionIndex");
    uint32_t actionIndex = (uint32_t)actionIndexValue->GetDoubleValue();

    STRIPID stripID = GetStripIDFromArgument();

    BEHAVIORSTATE stateID = GetBehaviorStateFromArgument();

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

    uint32_t behaviorID = GetBehaviorIDFromArgument();

    modelComponent->AddAction(stateID, stripID, type, valueParameterName, valueParameterString, valueParameterDouble, "", actionIndex, behaviorID);
}

void ControlBehaviors::MigrateActions() {
    Game::logger->Log("ControlBehaviors", "migrateActions!\n");
    
    AMFDoubleValue* srcActionIndexValue = (AMFDoubleValue*)arguments->FindValue("srcActionIndex");
    uint32_t srcActionIndex = (uint32_t)srcActionIndexValue->GetDoubleValue();

    STRIPID srcStripID = GetStripIDFromArgument("srcStripID");

    BEHAVIORSTATE srcStateID = GetBehaviorStateFromArgument("srcStateID");

    AMFDoubleValue* dstActionIndexValue = (AMFDoubleValue*)arguments->FindValue("dstActionIndex");
    uint32_t dstActionIndex = (uint32_t)dstActionIndexValue->GetDoubleValue();

    STRIPID dstStripID = GetStripIDFromArgument("dstStripID");

    BEHAVIORSTATE dstStateID = GetBehaviorStateFromArgument("dstStateID");

    uint32_t behaviorID = GetBehaviorIDFromArgument();

    modelComponent->MigrateActions(srcActionIndex, srcStripID, srcStateID, dstActionIndex, dstStripID, dstStateID, behaviorID);
}

void ControlBehaviors::RearrangeStrip() {
    Game::logger->Log("ControlBehaviors", "rearrangeStrip!\n");

    AMFDoubleValue* srcActionIndexValue = (AMFDoubleValue*)arguments->FindValue("srcActionIndex");
    uint32_t srcActionIndex = (uint32_t)srcActionIndexValue->GetDoubleValue();

    uint32_t stripID = GetStripIDFromArgument();

    uint32_t behaviorID = GetBehaviorIDFromArgument();

    AMFDoubleValue* dstActionIndexValue = (AMFDoubleValue*)arguments->FindValue("dstActionIndex");
    uint32_t dstActionIndex = (uint32_t)dstActionIndexValue->GetDoubleValue();

    BEHAVIORSTATE stateID = GetBehaviorStateFromArgument();

    modelComponent->RearrangeStrip(stateID, stripID, srcActionIndex, dstActionIndex, behaviorID);
}

void ControlBehaviors::Add() {
    Game::logger->Log("ControlBehaviors", "add!\n");

    uint32_t behaviorID = GetBehaviorIDFromArgument();

    uint32_t behaviorIndex = 0;
    AMFValue* behaviorIndexValue = arguments->FindValue("BehaviorIndex");

    if (behaviorIndexValue) {
        AMFDoubleValue* behaviorIndexValueConverted = (AMFDoubleValue*)behaviorIndexValue;
        behaviorIndex = (uint32_t)behaviorIndexValueConverted->GetDoubleValue();
    }

    modelComponent->AddBehavior(behaviorID, behaviorIndex);
    SendBehaviorListToClient();
}

void ControlBehaviors::RemoveActions() {
    Game::logger->Log("ControlBehaviors", "removeActions!\n");

    uint32_t behaviorID = GetBehaviorIDFromArgument();

    AMFDoubleValue* actionIndexValue = (AMFDoubleValue*)arguments->FindValue("actionIndex");
    uint32_t actionIndex = (uint32_t)actionIndexValue->GetDoubleValue();

    STRIPID stripID = GetStripIDFromArgument();

    BEHAVIORSTATE stateID = GetBehaviorStateFromArgument();
    modelComponent->RemoveAction(stateID, stripID, actionIndex, behaviorID);
}

void ControlBehaviors::Rename() {
    Game::logger->Log("ControlBehaviors", "rename!\n");

    uint32_t behaviorID = GetBehaviorIDFromArgument();

    AMFStringValue* nameAsValue = (AMFStringValue*)arguments->FindValue("Name");
    auto name = nameAsValue->GetStringValue();

    modelComponent->Rename(behaviorID, name);

    SendBehaviorListToClient();
}

// TODO This is also supposed to serialize the state of the behaviors in progress but those aren't implemented yet
void ControlBehaviors::SendBehaviorBlocksToClient() {
    Game::logger->Log("ControlBehaviors", "sendBehaviorBlocksToClient!\n");

    uint32_t behaviorID = GetBehaviorIDFromArgument();

    auto modelBehavior = modelComponent->FindBehavior(behaviorID);

    if (!modelBehavior) return;

    modelBehavior->VerifyStates();

    auto states = modelBehavior->GetBehaviorStates();

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
        auto stripsInState = it->second->GetStrips();
        for (auto strip = stripsInState.begin(); strip != stripsInState.end(); strip++) {
            Game::logger->Log("PropertyBehaviors", "Begin serialization of strip %i!\n", strip->first);
            AMFArrayValue* thisStrip = new AMFArrayValue();

            AMFDoubleValue* stripID = new AMFDoubleValue();
            stripID->SetDoubleValue(strip->first);
            thisStrip->InsertValue("id", stripID);

            AMFArrayValue* uiArray = new AMFArrayValue();
            AMFDoubleValue* yPosition = new AMFDoubleValue();
            yPosition->SetDoubleValue(strip->second->GetYPosition());
            uiArray->InsertValue("y", yPosition);

            AMFDoubleValue* xPosition = new AMFDoubleValue();
            xPosition->SetDoubleValue(strip->second->GetXPosition());
            uiArray->InsertValue("x", xPosition);
            
            thisStrip->InsertValue("ui", uiArray);
            targetObjectID = modelComponent->GetParent()->GetObjectID();
            behaviorID = modelBehavior->GetBehaviorID();

            AMFArrayValue* stripSerialize = new AMFArrayValue();
            for (auto behaviorAction : strip->second->GetActions()) {
                Game::logger->Log("PropertyBehaviors", "Begin serialization of action %s!\n", behaviorAction->actionName.c_str());
                AMFArrayValue* thisAction = new AMFArrayValue();

                AMFStringValue* actionName = new AMFStringValue();
                actionName->SetStringValue(behaviorAction->actionName);
                thisAction->InsertValue("Type", actionName);

                if (behaviorAction->parameterValueString != "")
                {
                    AMFStringValue* valueAsString = new AMFStringValue();
                    valueAsString->SetStringValue(behaviorAction->parameterValueString);
                    thisAction->InsertValue(behaviorAction->parameterName, valueAsString);
                } 
                else if (behaviorAction->parameterValueDouble != 0.0)
                {
                    AMFDoubleValue* valueAsDouble = new AMFDoubleValue();
                    valueAsDouble->SetDoubleValue(behaviorAction->parameterValueDouble);
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

void ControlBehaviors::UpdateAction() {
    Game::logger->Log("ControlBehaviors", "updateAction!\n");

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

    uint32_t behaviorID = GetBehaviorIDFromArgument();

    AMFDoubleValue* actionIndexValue = (AMFDoubleValue*)arguments->FindValue("actionIndex");
    uint32_t actionIndex = (uint32_t)actionIndexValue->GetDoubleValue();

    STRIPID stripID = GetStripIDFromArgument();

    BEHAVIORSTATE stateID = GetBehaviorStateFromArgument();

    modelComponent->UpdateAction(stateID, stripID, type, valueParameterName, valueParameterString, valueParameterDouble, "", actionIndex, behaviorID);
}

// TODO This doesn't save behaviors to the inventory at the moment, just removes them from the list of behaviors.
void ControlBehaviors::MoveToInventory() {
    Game::logger->Log("ControlBehaviors", "moveToInventory!\n");
    
    // This closes the UI menu should it be open while the player is removing behaviors
    AMFArrayValue args;
    
    AMFFalseValue* stateToPop = new AMFFalseValue();
    args.InsertValue("visible", stateToPop);

    GameMessages::SendUIMessageServerToSingleClient(modelOwner, modelOwner->GetParentUser()->GetSystemAddress(), "ToggleBehaviorEditor", &args);

    uint32_t behaviorID = GetBehaviorIDFromArgument();

    AMFDoubleValue* behaviorIndexValue = (AMFDoubleValue*)arguments->FindValue("BehaviorIndex");
    uint32_t behaviorIndex = (uint32_t)behaviorIndexValue->GetDoubleValue();

    modelComponent->MoveBehaviorToInventory(behaviorID, behaviorIndex);

    SendBehaviorListToClient();
}

uint32_t ControlBehaviors::GetBehaviorIDFromArgument(std::string key) {
    AMFValue* behaviorIDValue = arguments->FindValue(key);
    uint32_t behaviorID = -1;

    if (!behaviorIDValue) throw std::invalid_argument("Unable to find behavior state from argument \"" + key + "\"");

    if (behaviorIDValue->GetValueType() != AMFValueType::AMFUndefined) {
        behaviorID = std::stoi(((AMFStringValue*)behaviorIDValue)->GetStringValue());
    }

    delete behaviorIDValue;
    behaviorIDValue = nullptr;

    return behaviorID;
}

BEHAVIORSTATE ControlBehaviors::GetBehaviorStateFromArgument(std::string key) {
    AMFDoubleValue* stateIDValue = (AMFDoubleValue*)arguments->FindValue(key);
    if (!stateIDValue) throw std::invalid_argument("Unable to find behavior state from argument \"" + key + "\"");

    BEHAVIORSTATE stateID = (BEHAVIORSTATE)stateIDValue->GetDoubleValue();

    delete stateIDValue;
    stateIDValue = nullptr;

    return stateID;
}

STRIPID ControlBehaviors::GetStripIDFromArgument(std::string key) {
    AMFDoubleValue* stripIDValue = (AMFDoubleValue*)arguments->FindValue(key);
    if (!stripIDValue) throw std::invalid_argument("Unable to find strip ID from argument \"" + key + "\"");

    STRIPID stripID = (STRIPID)stripIDValue->GetDoubleValue();

    delete stripIDValue;
    stripIDValue = nullptr;

    return stripID;
}
