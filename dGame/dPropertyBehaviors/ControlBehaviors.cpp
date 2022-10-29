#include "ControlBehaviors.h"

#include "AMFFormat.h"
#include "Entity.h"
#include "Game.h"
#include "GameMessages.h"
#include "ModelComponent.h"
#include "../../dWorldServer/ObjectIDManager.h"
#include "dLogger.h"

//! Private Functions!

uint32_t GetBehaviorIDFromArgument(AMFArrayValue* arguments, const std::string& key = "behaviorID") {
	AMFValue* behaviorIDValue = arguments->FindValue(key);
	uint32_t behaviorID = -1;

	if (!behaviorIDValue) throw std::invalid_argument("Unable to find behavior state from argument \"" + key + "\"");

	if (behaviorIDValue->GetValueType() == AMFValueType::AMFString) {
		behaviorID = std::stoul((static_cast<AMFStringValue*>(behaviorIDValue))->GetStringValue());
	}

	return behaviorID;
}

BEHAVIORSTATE GetBehaviorStateFromArgument(AMFArrayValue* arguments, const std::string& key = "stateID") {
	AMFDoubleValue* stateIDValue = (AMFDoubleValue*)arguments->FindValue(key);
	if (!stateIDValue) throw std::invalid_argument("Unable to find behavior state from argument \"" + key + "\"");

	BEHAVIORSTATE stateID = (BEHAVIORSTATE)stateIDValue->GetDoubleValue();

	delete stateIDValue;
	stateIDValue = nullptr;

	return stateID;
}

STRIPID GetStripIDFromArgument(AMFArrayValue* arguments, const std::string& key = "stripID") {
	AMFDoubleValue* stripIDValue = (AMFDoubleValue*)arguments->FindValue(key);
	if (!stripIDValue) throw std::invalid_argument("Unable to find strip ID from argument \"" + key + "\"");

	STRIPID stripID = (STRIPID)stripIDValue->GetDoubleValue();

	delete stripIDValue;
	stripIDValue = nullptr;

	return stripID;
}

void RequestUpdatedID(int32_t behaviorID, ModelComponent* modelComponent, Entity* modelOwner, const SystemAddress& sysAddr) {
	// auto behavior = modelComponent->FindBehavior(behaviorID);
	// if (behavior->GetBehaviorID() == -1 || behavior->GetShouldSetNewID()) {
	// 	ObjectIDManager::Instance()->RequestPersistentID(
	// 		[behaviorID, behavior, modelComponent, modelOwner, sysAddr](uint32_t persistentId) {
	// 		behavior->SetShouldGetNewID(false);
	// 		behavior->SetIsTemplated(false);
	// 		behavior->SetBehaviorID(persistentId);

	// 		// This updates the behavior ID of the behavior should this be a new behavior
	// 		AMFArrayValue args;

	// 		AMFStringValue* behaviorIDString = new AMFStringValue();
	// 		behaviorIDString->SetStringValue(std::to_string(persistentId));
	// 		args.InsertValue("behaviorID", behaviorIDString);

	// 		AMFStringValue* objectIDAsString = new AMFStringValue();
	// 		objectIDAsString->SetStringValue(std::to_string(modelComponent->GetParent()->GetObjectID()));
	// 		args.InsertValue("objectID", objectIDAsString);

	// 		GameMessages::SendUIMessageServerToSingleClient(modelOwner, sysAddr, "UpdateBehaviorID", &args);
	// 		delete behaviorIDString;
	// 		behaviorIDString = nullptr;
	// 		delete objectIDAsString;
	// 		objectIDAsString = nullptr;
	// 		ControlBehaviors::SendBehaviorListToClient(modelComponent->GetParent(), sysAddr, modelOwner);
	// 	});
	// }
}

//! End of Private Functions!

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

void ControlBehaviors::ModelTypeChanged(AMFArrayValue* arguments, ModelComponent* ModelComponent) {
	AMFValue* modelTypeValue = arguments->FindValue("ModelType");
	if (modelTypeValue->GetValueType() != AMFValueType::AMFDouble) return;
	auto* requestedModelType = static_cast<AMFDoubleValue*>(modelTypeValue);
	uint32_t modelType = static_cast<uint32_t>(requestedModelType->GetDoubleValue());

	//TODO Update the model type here
}

void ControlBehaviors::ToggleExecutionUpdates() {

}

void ControlBehaviors::AddStrip(AMFArrayValue* arguments) {
	AMFArrayValue* strip = (AMFArrayValue*)arguments->FindValue("strip");

	AMFArrayValue* actions = (AMFArrayValue*)strip->FindValue("actions");

	AMFArrayValue* uiArray = (AMFArrayValue*)arguments->FindValue("ui");

	AMFDoubleValue* xPositionValue = (AMFDoubleValue*)uiArray->FindValue("x");
	double xPosition = xPositionValue->GetDoubleValue();

	AMFDoubleValue* yPositionValue = (AMFDoubleValue*)uiArray->FindValue("y");
	double yPosition = yPositionValue->GetDoubleValue();

	STRIPID stripID = GetStripIDFromArgument(arguments);

	BEHAVIORSTATE stateID = GetBehaviorStateFromArgument(arguments);

	uint32_t behaviorID = GetBehaviorIDFromArgument(arguments);

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
		// modelComponent->AddStrip(stateID, stripID, type, behaviorID, valueParameterName, valueParameterString, valueParameterDouble, "", xPosition, yPosition);
		type = "";
		valueParameterName = "";
		valueParameterString = "";
		valueParameterDouble = 0.0;
	}
	// RequestUpdatedID(behaviorID);
}

void ControlBehaviors::RemoveStrip(AMFArrayValue* arguments) {
	STRIPID stripID = GetStripIDFromArgument(arguments);

	BEHAVIORSTATE stateID = GetBehaviorStateFromArgument(arguments);

	uint32_t behaviorID = GetBehaviorIDFromArgument(arguments);

	// modelComponent->RemoveStrip(stateID, stripID, behaviorID);

	// RequestUpdatedID(behaviorID);
}

void ControlBehaviors::MergeStrips(AMFArrayValue* arguments) {
	STRIPID srcStripID = GetStripIDFromArgument(arguments, "srcStripID");

	BEHAVIORSTATE dstStateID = GetBehaviorStateFromArgument(arguments, "dstStateID");

	BEHAVIORSTATE srcStateID = GetBehaviorStateFromArgument(arguments, "srcStateID");

	AMFDoubleValue* dstActionIndexValue = (AMFDoubleValue*)arguments->FindValue("dstActionIndex");
	uint32_t dstActionIndex = (uint32_t)dstActionIndexValue->GetDoubleValue();

	STRIPID dstStripID = GetStripIDFromArgument(arguments, "dstStripID");

	uint32_t behaviorID = GetBehaviorIDFromArgument(arguments);

	// modelComponent->MergeStrips(srcStripID, dstStripID, srcStateID, dstStateID, behaviorID, dstActionIndex);

	// RequestUpdatedID(behaviorID);
}

void ControlBehaviors::SplitStrip(AMFArrayValue* arguments) {
	AMFDoubleValue* srcActionIndexValue = (AMFDoubleValue*)arguments->FindValue("srcActionIndex");
	uint32_t srcActionIndex = (uint32_t)srcActionIndexValue->GetDoubleValue();

	STRIPID srcStripID = GetStripIDFromArgument(arguments, "srcStripID");

	BEHAVIORSTATE srcStateID = GetBehaviorStateFromArgument(arguments, "srcStateID");

	STRIPID dstStripID = GetStripIDFromArgument(arguments, "dstStripID");

	BEHAVIORSTATE dstStateID = GetBehaviorStateFromArgument(arguments, "dstStateID");

	AMFArrayValue* dstStripUIArray = (AMFArrayValue*)arguments->FindValue("dstStripUI");

	AMFDoubleValue* xPositionValue = (AMFDoubleValue*)dstStripUIArray->FindValue("x");
	AMFDoubleValue* yPositionValue = (AMFDoubleValue*)dstStripUIArray->FindValue("y");
	// x and y position 15 are just where the game puts the strip by default if none is given.
	double yPosition = yPositionValue != nullptr ? yPositionValue->GetDoubleValue() : 15;
	double xPosition = xPositionValue != nullptr ? xPositionValue->GetDoubleValue() : 15;

	uint32_t behaviorID = GetBehaviorIDFromArgument(arguments);

	// modelComponent->SplitStrip(srcActionIndex, srcStripID, srcStateID, dstStripID, dstStateID, behaviorID, yPosition, xPosition);

	// RequestUpdatedID(behaviorID);
}

void ControlBehaviors::UpdateStripUI(AMFArrayValue* arguments) {
	AMFArrayValue* uiArray = (AMFArrayValue*)arguments->FindValue("ui");

	AMFDoubleValue* xPositionValue = (AMFDoubleValue*)uiArray->FindValue("x");
	AMFDoubleValue* yPositionValue = (AMFDoubleValue*)uiArray->FindValue("y");
	double yPosition = yPositionValue->GetDoubleValue();
	double xPosition = xPositionValue->GetDoubleValue();

	STRIPID stripID = GetStripIDFromArgument(arguments);

	BEHAVIORSTATE stateID = GetBehaviorStateFromArgument(arguments);

	uint32_t behaviorID = GetBehaviorIDFromArgument(arguments);

	// modelComponent->UpdateUIOfStrip(stateID, stripID, xPosition, yPosition, behaviorID);

	// RequestUpdatedID(behaviorID);
}

void ControlBehaviors::AddAction(AMFArrayValue* arguments) {
	AMFDoubleValue* actionIndexValue = (AMFDoubleValue*)arguments->FindValue("actionIndex");
	uint32_t actionIndex = (uint32_t)actionIndexValue->GetDoubleValue();

	STRIPID stripID = GetStripIDFromArgument(arguments);

	BEHAVIORSTATE stateID = GetBehaviorStateFromArgument(arguments);

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

	uint32_t behaviorID = GetBehaviorIDFromArgument(arguments);

	// modelComponent->AddAction(stateID, stripID, type, valueParameterName, valueParameterString, valueParameterDouble, "", actionIndex, behaviorID);

	// RequestUpdatedID(behaviorID);
}

void ControlBehaviors::MigrateActions(AMFArrayValue* arguments) {
	AMFDoubleValue* srcActionIndexValue = (AMFDoubleValue*)arguments->FindValue("srcActionIndex");
	uint32_t srcActionIndex = (uint32_t)srcActionIndexValue->GetDoubleValue();

	STRIPID srcStripID = GetStripIDFromArgument(arguments, "srcStripID");

	BEHAVIORSTATE srcStateID = GetBehaviorStateFromArgument(arguments, "srcStateID");

	AMFDoubleValue* dstActionIndexValue = (AMFDoubleValue*)arguments->FindValue("dstActionIndex");
	uint32_t dstActionIndex = (uint32_t)dstActionIndexValue->GetDoubleValue();

	STRIPID dstStripID = GetStripIDFromArgument(arguments, "dstStripID");

	BEHAVIORSTATE dstStateID = GetBehaviorStateFromArgument(arguments, "dstStateID");

	uint32_t behaviorID = GetBehaviorIDFromArgument(arguments);

	// modelComponent->MigrateActions(srcActionIndex, srcStripID, srcStateID, dstActionIndex, dstStripID, dstStateID, behaviorID);

	// RequestUpdatedID(behaviorID);
}

void ControlBehaviors::RearrangeStrip(AMFArrayValue* arguments) {
	AMFDoubleValue* srcActionIndexValue = (AMFDoubleValue*)arguments->FindValue("srcActionIndex");
	uint32_t srcActionIndex = (uint32_t)srcActionIndexValue->GetDoubleValue();

	uint32_t stripID = GetStripIDFromArgument(arguments);

	uint32_t behaviorID = GetBehaviorIDFromArgument(arguments);

	AMFDoubleValue* dstActionIndexValue = (AMFDoubleValue*)arguments->FindValue("dstActionIndex");
	uint32_t dstActionIndex = (uint32_t)dstActionIndexValue->GetDoubleValue();

	BEHAVIORSTATE stateID = GetBehaviorStateFromArgument(arguments);

	// modelComponent->RearrangeStrip(stateID, stripID, srcActionIndex, dstActionIndex, behaviorID);

	// RequestUpdatedID(behaviorID);
}

void ControlBehaviors::Add(AMFArrayValue* arguments) {
	uint32_t behaviorID = GetBehaviorIDFromArgument(arguments);

	uint32_t behaviorIndex = 0;
	AMFValue* behaviorIndexValue = arguments->FindValue("BehaviorIndex");

	if (behaviorIndexValue) {
		AMFDoubleValue* behaviorIndexValueConverted = (AMFDoubleValue*)behaviorIndexValue;
		behaviorIndex = (uint32_t)behaviorIndexValueConverted->GetDoubleValue();
	}

	// modelComponent->AddBehavior(behaviorID, behaviorIndex, modelOwner);
	// SendBehaviorListToClient();
}

void ControlBehaviors::RemoveActions(AMFArrayValue* arguments) {
	uint32_t behaviorID = GetBehaviorIDFromArgument(arguments);

	AMFDoubleValue* actionIndexValue = (AMFDoubleValue*)arguments->FindValue("actionIndex");
	uint32_t actionIndex = (uint32_t)actionIndexValue->GetDoubleValue();

	STRIPID stripID = GetStripIDFromArgument(arguments);

	BEHAVIORSTATE stateID = GetBehaviorStateFromArgument(arguments);
	// modelComponent->RemoveAction(stateID, stripID, actionIndex, behaviorID);

	// RequestUpdatedID(behaviorID);
}

void ControlBehaviors::Rename(Entity* modelEntity, const SystemAddress& sysAddr, Entity* modelOwner, AMFArrayValue* arguments) {
	uint32_t behaviorID = GetBehaviorIDFromArgument(arguments);

	AMFStringValue* nameAsValue = (AMFStringValue*)arguments->FindValue("Name");
	auto name = nameAsValue->GetStringValue();

	// modelComponent->Rename(behaviorID, name);

	SendBehaviorListToClient(modelEntity, sysAddr, modelOwner);

	// RequestUpdatedID(behaviorID);
}

// TODO This is also supposed to serialize the state of the behaviors in progress but those aren't implemented yet
void ControlBehaviors::SendBehaviorBlocksToClient(ModelComponent* modelComponent, const SystemAddress& sysAddr, Entity* modelOwner, AMFArrayValue* arguments) {
	uint32_t behaviorID = GetBehaviorIDFromArgument(arguments);

	// auto modelBehavior = modelComponent->FindBehavior(behaviorID);

	// if (!modelBehavior) return;

	// modelBehavior->VerifyStates();

	// auto states = modelBehavior->GetBehaviorStates();

	// // Begin serialization.

	// /**
	//  * for each state
	//  *	  strip id
	//  *	  ui info
	//  *		  x
	//  *		  y
	//  *	  actions
	//  *		  action1
	//  *		  action2
	//  *		  ...
	//  * behaviorID of strip
	//  * objectID of strip
	//  */
	// LWOOBJID targetObjectID = LWOOBJID_EMPTY;
	// behaviorID = 0;
	// AMFArrayValue behaviorInfo;

	// AMFArrayValue* stateSerialize = new AMFArrayValue();

	// for (auto it = states.begin(); it != states.end(); it++) {
	// 	Game::logger->Log("PropertyBehaviors", "Begin serialization of state %i!\n", it->first);
	// 	AMFArrayValue* state = new AMFArrayValue();

	// 	AMFDoubleValue* stateAsDouble = new AMFDoubleValue();
	// 	stateAsDouble->SetDoubleValue(it->first);
	// 	state->InsertValue("id", stateAsDouble);

	// 	AMFArrayValue* strips = new AMFArrayValue();
	// 	auto stripsInState = it->second->GetStrips();
	// 	for (auto strip = stripsInState.begin(); strip != stripsInState.end(); strip++) {
	// 		Game::logger->Log("PropertyBehaviors", "Begin serialization of strip %i!\n", strip->first);
	// 		AMFArrayValue* thisStrip = new AMFArrayValue();

	// 		AMFDoubleValue* stripID = new AMFDoubleValue();
	// 		stripID->SetDoubleValue(strip->first);
	// 		thisStrip->InsertValue("id", stripID);

	// 		AMFArrayValue* uiArray = new AMFArrayValue();
	// 		AMFDoubleValue* yPosition = new AMFDoubleValue();
	// 		yPosition->SetDoubleValue(strip->second->GetYPosition());
	// 		uiArray->InsertValue("y", yPosition);

	// 		AMFDoubleValue* xPosition = new AMFDoubleValue();
	// 		xPosition->SetDoubleValue(strip->second->GetXPosition());
	// 		uiArray->InsertValue("x", xPosition);

	// 		thisStrip->InsertValue("ui", uiArray);
	// 		targetObjectID = modelComponent->GetParent()->GetObjectID();
	// 		behaviorID = modelBehavior->GetBehaviorID();

	// 		AMFArrayValue* stripSerialize = new AMFArrayValue();
	// 		for (auto behaviorAction : strip->second->GetActions()) {
	// 			Game::logger->Log("PropertyBehaviors", "Begin serialization of action %s!\n", behaviorAction->actionName.c_str());
	// 			AMFArrayValue* thisAction = new AMFArrayValue();

	// 			AMFStringValue* actionName = new AMFStringValue();
	// 			actionName->SetStringValue(behaviorAction->actionName);
	// 			thisAction->InsertValue("Type", actionName);

	// 			if (behaviorAction->parameterValueString != "")
	// 			{
	// 				AMFStringValue* valueAsString = new AMFStringValue();
	// 				valueAsString->SetStringValue(behaviorAction->parameterValueString);
	// 				thisAction->InsertValue(behaviorAction->parameterName, valueAsString);
	// 			}
	// 			else if (behaviorAction->parameterValueDouble != 0.0)
	// 			{
	// 				AMFDoubleValue* valueAsDouble = new AMFDoubleValue();
	// 				valueAsDouble->SetDoubleValue(behaviorAction->parameterValueDouble);
	// 				thisAction->InsertValue(behaviorAction->parameterName, valueAsDouble);
	// 			}
	// 			stripSerialize->PushBackValue(thisAction);
	// 		}
	// 		thisStrip->InsertValue("actions", stripSerialize);
	// 		strips->PushBackValue(thisStrip);
	// 	}
	// 	state->InsertValue("strips", strips);
	// 	stateSerialize->PushBackValue(state);
	// }
	// behaviorInfo.InsertValue("states", stateSerialize);

	// AMFStringValue* objectidAsString = new AMFStringValue();
	// objectidAsString->SetStringValue(std::to_string(targetObjectID));
	// behaviorInfo.InsertValue("objectID", objectidAsString);

	// AMFStringValue* behaviorIDAsString = new AMFStringValue();
	// behaviorIDAsString->SetStringValue(std::to_string(behaviorID));
	// behaviorInfo.InsertValue("BehaviorID", behaviorIDAsString);

	// GameMessages::SendUIMessageServerToSingleClient(modelOwner, sysAddr, "UpdateBehaviorBlocks", &behaviorInfo);
}

void ControlBehaviors::UpdateAction(AMFArrayValue* arguments) {
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

	uint32_t behaviorID = GetBehaviorIDFromArgument(arguments);

	AMFDoubleValue* actionIndexValue = (AMFDoubleValue*)arguments->FindValue("actionIndex");
	uint32_t actionIndex = (uint32_t)actionIndexValue->GetDoubleValue();

	STRIPID stripID = GetStripIDFromArgument(arguments);

	BEHAVIORSTATE stateID = GetBehaviorStateFromArgument(arguments);

	// modelComponent->UpdateAction(stateID, stripID, type, valueParameterName, valueParameterString, valueParameterDouble, "", actionIndex, behaviorID);

	// RequestUpdatedID(behaviorID);
}

void ControlBehaviors::MoveToInventory(ModelComponent* modelComponent, const SystemAddress& sysAddr, Entity* modelOwner, AMFArrayValue* arguments) {
	// This closes the UI menu should it be open while the player is removing behaviors
	AMFArrayValue args;

	AMFFalseValue* stateToPop = new AMFFalseValue();
	args.InsertValue("visible", stateToPop);

	GameMessages::SendUIMessageServerToSingleClient(modelOwner, modelOwner->GetParentUser()->GetSystemAddress(), "ToggleBehaviorEditor", &args);

	uint32_t behaviorID = GetBehaviorIDFromArgument(arguments);

	AMFDoubleValue* behaviorIndexValue = (AMFDoubleValue*)arguments->FindValue("BehaviorIndex");
	uint32_t behaviorIndex = (uint32_t)behaviorIndexValue->GetDoubleValue();

	// modelComponent->MoveBehaviorToInventory(behaviorID, behaviorIndex, modelOwner);

	SendBehaviorListToClient(modelComponent->GetParent(), sysAddr, modelOwner);
}
