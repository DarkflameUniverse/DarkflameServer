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
    AMFArrayValue behavior;
    AMFStringValue* behaviorID = new AMFStringValue();

    AMFStringValue* amfStringValueForObjectID = new AMFStringValue();
	amfStringValueForObjectID->SetStringValue(std::to_string(entity->GetObjectID()));

    behaviorID->SetStringValue("10447");

    AMFArrayValue* outerStates = new AMFArrayValue();

    AMFArrayValue* innerStates = new AMFArrayValue();

    AMFArrayValue* outerStrips = new AMFArrayValue();

    AMFArrayValue* innerStrips = new AMFArrayValue();

    AMFArrayValue* uiPosition = new AMFArrayValue();

    AMFDoubleValue* uiXPos = new AMFDoubleValue();
    AMFDoubleValue* uiYPos = new AMFDoubleValue();

    uiXPos->SetDoubleValue(103.0f);
    uiYPos->SetDoubleValue(82.0f);

    uiPosition->InsertValue("x", uiXPos);
    uiPosition->InsertValue("y", uiYPos);

    innerStrips->InsertValue("ui", uiPosition);

    AMFArrayValue* behaviorBlock1 = new AMFArrayValue();
    AMFStringValue* emptyString = new AMFStringValue();
    emptyString->SetStringValue("");
    behaviorBlock1->InsertValue("__callbackID__", emptyString);
    AMFStringValue* onInteract = new AMFStringValue();
    onInteract->SetStringValue("OnInteract");
    behaviorBlock1->InsertValue("Type", onInteract);

    AMFArrayValue* behaviorBlock2 = new AMFArrayValue();
    behaviorBlock2->InsertValue("__callbackID__", emptyString);
    AMFDoubleValue* distance = new AMFDoubleValue();
    distance->SetDoubleValue(25.0);
    behaviorBlock2->InsertValue("Distance", distance);
    AMFStringValue* flyUp = new AMFStringValue();
    flyUp->SetStringValue("FlyUp");
    behaviorBlock2->InsertValue("Type", flyUp);

    AMFArrayValue* behaviorBlock3 = new AMFArrayValue();
    behaviorBlock3->InsertValue("__callbackID__", emptyString);
    AMFStringValue* flyDown = new AMFStringValue();
    flyDown->SetStringValue("FlyDown");
    behaviorBlock3->InsertValue("Type", flyDown);
    behaviorBlock3->InsertValue("Distance", distance);

    AMFArrayValue* actionsList = new AMFArrayValue();
    actionsList->PushBackValue(behaviorBlock1);
    actionsList->PushBackValue(behaviorBlock2);
    actionsList->PushBackValue(behaviorBlock3);

    innerStrips->InsertValue("actions", actionsList);
    AMFDoubleValue* zeroDouble = new AMFDoubleValue();
    zeroDouble->SetDoubleValue(0.0f);
    innerStrips->InsertValue("id", zeroDouble);
    
    innerStates->InsertValue("strips", innerStrips);

    innerStates->InsertValue("id", zeroDouble);

    // outerStates->InsertValue("states", innerStates);
    behavior.InsertValue("states", innerStates);
    behavior.InsertValue("BehaviorID", behaviorID);

    AMFArrayValue* firstInnerStrips = new AMFArrayValue();
    firstInnerStrips->InsertValue("id", zeroDouble);
    firstInnerStrips->InsertValue("actionIndex", zeroDouble);

    AMFArrayValue* firstOuterStrips = new AMFArrayValue();
    firstOuterStrips->PushBackValue(firstInnerStrips);

    AMFArrayValue* executionState = new AMFArrayValue();
    executionState->InsertValue("stateID", zeroDouble);
    executionState->InsertValue("strips", firstOuterStrips);
    behavior.InsertValue("executionState", executionState);
    behavior.InsertValue("objectID", amfStringValueForObjectID);

    GameMessages::SendUIMessageServerToSingleClient(entity, sysAddr, "UpdateBehaviorList", &behavior);
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
