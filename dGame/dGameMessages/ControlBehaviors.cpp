#include "ControlBehaviors.h"

void ControlBehaviors::DoActions(Entity* entity, const SystemAddress& sysAddr, AMFArrayValue* arguments, std::string command) {
    if (command == "sendBehaviorListToClient")
        sendBehaviorListToClient(entity, sysAddr, arguments);
    else if (command == "modelTypeChanged")
        modelTypeChanged(entity, sysAddr, arguments);
    else if (command == "toggleExecutionUpdates")
        toggleExecutionUpdates(entity, sysAddr, arguments); // UpdateBehaviorBlocks
    else if (command == "addStrip")
        addStrip(entity, sysAddr, arguments);
    else if (command == "removeStrip")
        removeStrip(entity, sysAddr, arguments);
    else if (command == "mergeStrips")
        mergeStrips(entity, sysAddr, arguments);
    else if (command == "splitStrip")
        splitStrip(entity, sysAddr, arguments);
    else if (command == "updateStripUI")
        updateStripUI(entity, sysAddr, arguments);
    else if (command == "addAction")
        addAction(entity, sysAddr, arguments);
    else if (command == "migrateActions")
        migrateActions(entity, sysAddr, arguments);
    else if (command == "rearrangeStrip")
        rearrangeStrip(entity, sysAddr, arguments);
    else if (command == "add")
        add(entity, sysAddr, arguments); // UpdateBehaviorBlocks
    else if (command == "removeActions")
        removeActions(entity, sysAddr, arguments);
}
// partially done
void ControlBehaviors::sendBehaviorListToClient(Entity* entity, const SystemAddress& sysAddr, AMFArrayValue* arguments) {
    Game::logger->Log("GameMessages", "Updating behavior list!\n");
    AMFArrayValue behaviorList;
    // For each behavior, get its info and add it to the list
    AMFArrayValue* behaviors = new AMFArrayValue();
	AMFArrayValue* behaviorInfo = new AMFArrayValue();

	AMFStringValue* amfStringValueForObjectID = new AMFStringValue();
	amfStringValueForObjectID->SetStringValue(std::to_string(entity->GetObjectID()));

	behaviorList.InsertValue("objectID", amfStringValueForObjectID);
	
	GameMessages::SendUIMessageServerToSingleClient(entity, sysAddr, "UpdateBehaviorList", &behaviorList);
}

void ControlBehaviors::modelTypeChanged(Entity* entity, const SystemAddress& sysAddr, AMFArrayValue* arguments) {

}

void ControlBehaviors::toggleExecutionUpdates(Entity* entity, const SystemAddress& sysAddr, AMFArrayValue* arguments) {
    double eightTwo = 82.0;
    double oneOThree = 103.0;
    double zero = 0.0;
    double value = 25.0;
    AMFArrayValue behaviorInfo;

    AMFArrayValue* executionState = new AMFArrayValue();

    AMFArrayValue* stripsInState = new AMFArrayValue();

    AMFArrayValue* aStripInProgress = new AMFArrayValue();

    AMFDoubleValue* zeroDouble = new AMFDoubleValue();
    zeroDouble->SetDoubleValue(zero);
    aStripInProgress->InsertValue("id", zeroDouble);
    aStripInProgress->InsertValue("actionIndex", zeroDouble);
    // End of strip info
    stripsInState->PushBackValue(aStripInProgress);
    executionState->InsertValue("strips", stripsInState);
    executionState->InsertValue("stateID", zeroDouble);
    // End of stripsInState
    behaviorInfo.InsertValue("executionState", executionState);

    // End of executionState
    AMFArrayValue* listOfStates = new AMFArrayValue();

    AMFArrayValue* homeStateInfo = new AMFArrayValue();

    homeStateInfo->InsertValue("id", zeroDouble);

    AMFArrayValue* homeStateStrips = new AMFArrayValue();

    AMFArrayValue* stripInfo = new AMFArrayValue();

    stripInfo->InsertValue("id", zeroDouble);

    AMFArrayValue* stripLocation = new AMFArrayValue();
    AMFDoubleValue* yLocation = new AMFDoubleValue();
    yLocation->SetDoubleValue(eightTwo);
    AMFDoubleValue* xLocation = new AMFDoubleValue();
    xLocation->SetDoubleValue(oneOThree);
    stripLocation->InsertValue("y", yLocation);
    stripLocation->InsertValue("x", xLocation);

    stripInfo->InsertValue("ui", stripLocation);

    AMFArrayValue* actionsInStrip = new AMFArrayValue();

    AMFArrayValue* action1 = new AMFArrayValue();

    AMFStringValue* emptyString1 = new AMFStringValue();
    emptyString1->SetStringValue("");
    action1->InsertValue("__callbackID__", emptyString1);
    AMFStringValue* onInteract = new AMFStringValue();
    onInteract->SetStringValue("OnInteract");
    action1->InsertValue("Type", onInteract);
    actionsInStrip->PushBackValue(action1);
    
    AMFArrayValue* action2 = new AMFArrayValue();
    AMFDoubleValue* distance1 = new AMFDoubleValue();
    distance1->SetDoubleValue(value);
    action2->InsertValue("Distance", distance1);
    AMFStringValue* emptyString2 = new AMFStringValue();
    emptyString2->SetStringValue("");
    action2->InsertValue("__callbackID__", emptyString2);
    AMFStringValue* flyUp = new AMFStringValue();
    flyUp->SetStringValue("FlyUp");
    action2->InsertValue("Type", flyUp);
    actionsInStrip->PushBackValue(action2);

    AMFArrayValue* action3 = new AMFArrayValue();
    AMFDoubleValue* distance2 = new AMFDoubleValue();
    distance2->SetDoubleValue(value);
    action3->InsertValue("Distance", distance2);
    AMFStringValue* emptyString3 = new AMFStringValue();
    emptyString3->SetStringValue("");
    action3->InsertValue("__callbackID__", emptyString3);
    AMFStringValue* flyDown = new AMFStringValue();
    flyDown->SetStringValue("FlyDown");
    action3->InsertValue("Type", flyDown);
    actionsInStrip->PushBackValue(action3);  

    stripInfo->InsertValue("actions", actionsInStrip);

    homeStateStrips->PushBackValue(stripInfo);

    homeStateInfo->InsertValue("strips", homeStateStrips);

    listOfStates->PushBackValue(homeStateInfo);

    behaviorInfo.InsertValue("states", listOfStates);

    AMFStringValue* behaviorID = new AMFStringValue();
    behaviorID->SetStringValue("10447");
    behaviorInfo.InsertValue("BehaviorID", behaviorID);
    AMFStringValue* objectID = new AMFStringValue();
    objectID->SetStringValue(std::to_string(entity->GetObjectID()));
    behaviorInfo.InsertValue("objectID", objectID);
    GameMessages::SendUIMessageServerToSingleClient(entity, sysAddr, "UpdateBehaviorBlocks", &behaviorInfo);
}

void ControlBehaviors::addStrip(Entity* entity, const SystemAddress& sysAddr, AMFArrayValue* arguments) {

}

void ControlBehaviors::removeStrip(Entity* entity, const SystemAddress& sysAddr, AMFArrayValue* arguments) {

}

void ControlBehaviors::mergeStrips(Entity* entity, const SystemAddress& sysAddr, AMFArrayValue* arguments) {

}

void ControlBehaviors::splitStrip(Entity* entity, const SystemAddress& sysAddr, AMFArrayValue* arguments) {

}

void ControlBehaviors::updateStripUI(Entity* entity, const SystemAddress& sysAddr, AMFArrayValue* arguments) {

}

void ControlBehaviors::addAction(Entity* entity, const SystemAddress& sysAddr, AMFArrayValue* arguments) {

}

void ControlBehaviors::migrateActions(Entity* entity, const SystemAddress& sysAddr, AMFArrayValue* arguments) {

}

void ControlBehaviors::rearrangeStrip(Entity* entity, const SystemAddress& sysAddr, AMFArrayValue* arguments) {

}

void ControlBehaviors::add(Entity* entity, const SystemAddress& sysAddr, AMFArrayValue* arguments) {

}

void ControlBehaviors::removeActions(Entity* entity, const SystemAddress& sysAddr, AMFArrayValue* arguments) {

}
