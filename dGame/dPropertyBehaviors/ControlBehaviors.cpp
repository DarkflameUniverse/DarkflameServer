#include "ControlBehaviors.h"

#include "AMFFormat.h"
#include "Entity.h"
#include "Game.h"
#include "GameMessages.h"
#include "ModelComponent.h"
#include "dLogger.h"

void ControlBehaviors::ProcessCommand(Entity* modelEntity, const SystemAddress& sysAddr, AMFArrayValue* arguments, std::string command, Entity* modelOwner) {
	if (!modelEntity || !modelOwner || !arguments) return;

    if (command == "sendBehaviorListToClient")
        SendBehaviorListToClient(modelEntity, sysAddr, modelOwner);
    else if (command == "modelTypeChanged")
        Game::logger->Log("ControlBehaviors", "Got command %s but is not implemented!", command.c_str());
    else if (command == "toggleExecutionUpdates")
        Game::logger->Log("ControlBehaviors", "Got command %s but is not implemented!", command.c_str());
    else if (command == "addStrip")
        Game::logger->Log("ControlBehaviors", "Got command %s but is not implemented!", command.c_str());
    else if (command == "removeStrip")
        Game::logger->Log("ControlBehaviors", "Got command %s but is not implemented!", command.c_str());
    else if (command == "mergeStrips")
        Game::logger->Log("ControlBehaviors", "Got command %s but is not implemented!", command.c_str());
    else if (command == "splitStrip")
        Game::logger->Log("ControlBehaviors", "Got command %s but is not implemented!", command.c_str());
    else if (command == "updateStripUI")
        Game::logger->Log("ControlBehaviors", "Got command %s but is not implemented!", command.c_str());
    else if (command == "addAction")
        Game::logger->Log("ControlBehaviors", "Got command %s but is not implemented!", command.c_str());
    else if (command == "migrateActions")
        Game::logger->Log("ControlBehaviors", "Got command %s but is not implemented!", command.c_str());
    else if (command == "rearrangeStrip")
        Game::logger->Log("ControlBehaviors", "Got command %s but is not implemented!", command.c_str());
    else if (command == "add")
        Game::logger->Log("ControlBehaviors", "Got command %s but is not implemented!", command.c_str());
    else if (command == "removeActions")
        Game::logger->Log("ControlBehaviors", "Got command %s but is not implemented!", command.c_str());
    else if (command == "rename")
        Game::logger->Log("ControlBehaviors", "Got command %s but is not implemented!", command.c_str());
    else if (command == "sendBehaviorBlocksToClient")
        Game::logger->Log("ControlBehaviors", "Got command %s but is not implemented!", command.c_str());
    else if (command == "moveToInventory")
        Game::logger->Log("ControlBehaviors", "Got command %s but is not implemented!", command.c_str());
    else if (command == "updateAction")
        Game::logger->Log("ControlBehaviors", "Got command %s but is not implemented!", command.c_str());
    else
        Game::logger->Log("ControlBehaviors", "Unknown behavior command (%s)\n", command.c_str());
}

void ControlBehaviors::SendBehaviorListToClient(
	Entity* modelEntity,
	const SystemAddress& sysAddr,
	Entity* modelOwner
	) {
	auto* modelComponent = modelEntity->GetComponent<ModelComponent>();

	if (!modelComponent) return;

    AMFArrayValue behaviorsToSerialize;

	AMFArrayValue* behaviors = new AMFArrayValue(); // Empty for now

	/**
	 * The behaviors AMFArray will have up to 5 elements in the dense portion.
	 * Each element in the dense portion will be made up of another AMFArray
	 * with the following information mapped in the associative portion
	 * "id": Behavior ID cast to an AMFString
	 * "isLocked": AMFTrue or AMFFalse of whether or not the behavior is locked
	 * "isLoot": AMFTrue or AMFFalse of whether or not the behavior is a custom behavior (true if custom)
	 * "name": The name of the behavior formatted as an AMFString
	 */

	behaviorsToSerialize.InsertValue("behaviors", behaviors);

	AMFStringValue* amfStringValueForObjectID = new AMFStringValue();
	amfStringValueForObjectID->SetStringValue(std::to_string(modelComponent->GetParent()->GetObjectID()));

	behaviorsToSerialize.InsertValue("objectID", amfStringValueForObjectID);
	GameMessages::SendUIMessageServerToSingleClient(modelOwner, sysAddr, "UpdateBehaviorList", &behaviorsToSerialize);
}
