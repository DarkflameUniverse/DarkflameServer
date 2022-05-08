#include "ControlBehaviors.h"
#include "PropertyBehaviors.h"

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
    // double eightTwo = 82.0;
    // double oneOThree = 103.0;
    // double zero = 0.0;
    // double value = 25.0;
    // AMFArrayValue behaviorInfo;

    // AMFArrayValue* executionState = new AMFArrayValue();

    // AMFArrayValue* stripsInState = new AMFArrayValue();

    // AMFArrayValue* aStripInProgress = new AMFArrayValue();

    // AMFDoubleValue* zeroDouble = new AMFDoubleValue();
    // zeroDouble->SetDoubleValue(zero);
    // aStripInProgress->InsertValue("id", zeroDouble);
    // aStripInProgress->InsertValue("actionIndex", zeroDouble);
    // // End of strip info
    // stripsInState->PushBackValue(aStripInProgress);
    // executionState->InsertValue("strips", stripsInState);
    // executionState->InsertValue("stateID", zeroDouble);
    // // End of stripsInState
    // behaviorInfo.InsertValue("executionState", executionState);

    // // End of executionState
    // AMFArrayValue* listOfStates = new AMFArrayValue();

    // AMFArrayValue* homeStateInfo = new AMFArrayValue();

    // homeStateInfo->InsertValue("id", zeroDouble);

    // AMFArrayValue* homeStateStrips = new AMFArrayValue();

    // AMFArrayValue* stripInfo = new AMFArrayValue();

    // stripInfo->InsertValue("id", zeroDouble);

    // AMFArrayValue* stripLocation = new AMFArrayValue();
    // AMFDoubleValue* yLocation = new AMFDoubleValue();
    // yLocation->SetDoubleValue(eightTwo);
    // AMFDoubleValue* xLocation = new AMFDoubleValue();
    // xLocation->SetDoubleValue(oneOThree);
    // stripLocation->InsertValue("y", yLocation);
    // stripLocation->InsertValue("x", xLocation);

    // stripInfo->InsertValue("ui", stripLocation);

    // AMFArrayValue* actionsInStrip = new AMFArrayValue();

    // AMFArrayValue* action1 = new AMFArrayValue();

    // AMFStringValue* emptyString1 = new AMFStringValue();
    // emptyString1->SetStringValue("");
    // action1->InsertValue("__callbackID__", emptyString1);
    // AMFStringValue* onInteract = new AMFStringValue();
    // onInteract->SetStringValue("OnInteract");
    // action1->InsertValue("Type", onInteract);
    // actionsInStrip->PushBackValue(action1);
    
    // AMFArrayValue* action2 = new AMFArrayValue();
    // AMFDoubleValue* distance1 = new AMFDoubleValue();
    // distance1->SetDoubleValue(value);
    // action2->InsertValue("Distance", distance1);
    // AMFStringValue* emptyString2 = new AMFStringValue();
    // emptyString2->SetStringValue("");
    // action2->InsertValue("__callbackID__", emptyString2);
    // AMFStringValue* flyUp = new AMFStringValue();
    // flyUp->SetStringValue("FlyUp");
    // action2->InsertValue("Type", flyUp);
    // actionsInStrip->PushBackValue(action2);

    // AMFArrayValue* action3 = new AMFArrayValue();
    // AMFDoubleValue* distance2 = new AMFDoubleValue();
    // distance2->SetDoubleValue(value);
    // action3->InsertValue("Distance", distance2);
    // AMFStringValue* emptyString3 = new AMFStringValue();
    // emptyString3->SetStringValue("");
    // action3->InsertValue("__callbackID__", emptyString3);
    // AMFStringValue* flyDown = new AMFStringValue();
    // flyDown->SetStringValue("FlyDown");
    // action3->InsertValue("Type", flyDown);
    // actionsInStrip->PushBackValue(action3);  

    // stripInfo->InsertValue("actions", actionsInStrip);

    // homeStateStrips->PushBackValue(stripInfo);

    // homeStateInfo->InsertValue("strips", homeStateStrips);

    // listOfStates->PushBackValue(homeStateInfo);

    // behaviorInfo.InsertValue("states", listOfStates);

    // AMFStringValue* behaviorID = new AMFStringValue();
    // behaviorID->SetStringValue("10447");
    // behaviorInfo.InsertValue("BehaviorID", behaviorID);
    // AMFStringValue* objectID = new AMFStringValue();
    // objectID->SetStringValue(std::to_string(modelEntity->GetObjectID()));
    // behaviorInfo.InsertValue("objectID", objectID);

    PropertyBehaviors pb;
    pb.SerializeStrip(modelEntity->GetObjectID(), sysAddr, modelOwner);

    // GameMessages::SendUIMessageServerToSingleClient(modelOwner, sysAddr, "UpdateBehaviorBlocks", &behaviorInfo);
}

void ControlBehaviors::addStrip(Entity* modelEntity, const SystemAddress& sysAddr, AMFArrayValue* arguments, Entity* modelOwner) {
    Game::logger->Log("ControlBehaviors", "addStrip!\n");
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