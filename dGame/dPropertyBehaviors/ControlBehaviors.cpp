#include "ControlBehaviors.h"

#include "AMFFormat.h"
#include "Entity.h"
#include "Game.h"
#include "GameMessages.h"
#include "ModelComponent.h"
#include "../../dWorldServer/ObjectIDManager.h"
#include "dLogger.h"
#include "BehaviorStates.h"

uint32_t GetBehaviorIDFromArgument(AMFArrayValue* arguments, const std::string& key = "BehaviorID") {
	auto* behaviorIDValue = arguments->FindValue<AMFStringValue>(key);
	uint32_t behaviorID = -1;

	if (behaviorIDValue) {
		behaviorID = std::stoul(behaviorIDValue->GetStringValue());
	} else if (arguments->FindValue<AMFUndefinedValue>(key) == nullptr){
		throw std::invalid_argument("Unable to find behavior ID from argument \"" + key + "\"");
	}

	return behaviorID;
}

BehaviorState GetBehaviorStateFromArgument(AMFArrayValue* arguments, const std::string& key = "stateID") {
	auto* stateIDValue = arguments->FindValue<AMFDoubleValue>(key);
	if (!stateIDValue) throw std::invalid_argument("Unable to find behavior state from argument \"" + key + "\"");

	BehaviorState stateID = static_cast<BehaviorState>(stateIDValue->GetDoubleValue());

	return stateID;
}

StripId GetStripIDFromArgument(AMFArrayValue* arguments, const std::string& key = "stripID") {
	auto* stripIDValue = arguments->FindValue<AMFDoubleValue>(key);
	if (!stripIDValue) throw std::invalid_argument("Unable to find strip ID from argument \"" + key + "\"");

	StripId stripID = static_cast<StripId>(stripIDValue->GetDoubleValue());

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
	// 		ControlBehaviors::SendBehaviorListToClient(modelComponent->GetParent(), sysAddr, modelOwner);
	// 	});
	// }
}

void SendBehaviorListToClient(
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

void ModelTypeChanged(AMFArrayValue* arguments, ModelComponent* ModelComponent) {
	auto* modelTypeAmf = arguments->FindValue<AMFDoubleValue>("ModelType");
	if (!modelTypeAmf) return;

	uint32_t modelType = static_cast<uint32_t>(modelTypeAmf->GetDoubleValue());

	//TODO Update the model type here
}

void ToggleExecutionUpdates() {
	//TODO do something with this info
}

void AddStrip(AMFArrayValue* arguments) {
	auto* strip = arguments->FindValue<AMFArrayValue>("strip");
	if (!strip) return;

	auto* actions = strip->FindValue<AMFArrayValue>("actions");
	if (!actions) return;

	auto* uiArray = arguments->FindValue<AMFArrayValue>("ui");
	if (!uiArray) return;

	auto* xPositionValue = uiArray->FindValue<AMFDoubleValue>("x");
	if (!xPositionValue) return;

	double xPosition = xPositionValue->GetDoubleValue();

	auto* yPositionValue = uiArray->FindValue<AMFDoubleValue>("y");
	if (!yPositionValue) return;

	double yPosition = yPositionValue->GetDoubleValue();

	StripId stripID = GetStripIDFromArgument(arguments);

	BehaviorState stateID = GetBehaviorStateFromArgument(arguments);

	uint32_t behaviorID = GetBehaviorIDFromArgument(arguments);

	std::string type = "";
	std::string valueParameterName = "";
	std::string valueParameterString = "";
	double valueParameterDouble = 0.0;
	for (uint32_t position = 0; position < actions->GetDenseValueSize(); position++) {
		auto* actionAsArray = actions->GetValueAt<AMFArrayValue>(position);
		if (!actionAsArray) continue;

		for (auto& typeValueMap : actionAsArray->GetAssociativeMap()) {
			if (typeValueMap.first == "Type") {
				if (typeValueMap.second->GetValueType() != AMFValueType::AMFString) continue;

				type = static_cast<AMFStringValue*>(typeValueMap.second)->GetStringValue();
			} else {
				valueParameterName = typeValueMap.first;
				// Message is the only known string parameter
				if (valueParameterName == "Message") {
					if (typeValueMap.second->GetValueType() != AMFValueType::AMFString) continue;
					valueParameterString = static_cast<AMFStringValue*>(typeValueMap.second)->GetStringValue();
				} else {
					if (typeValueMap.second->GetValueType() != AMFValueType::AMFDouble) continue;
					valueParameterDouble = static_cast<AMFDoubleValue*>(typeValueMap.second)->GetDoubleValue();
				}
			}
		}
		// modelComponent->AddStrip(stateID, stripID, type, behaviorID, valueParameterName, valueParameterString, valueParameterDouble, "", xPosition, yPosition);
		type.clear();
		valueParameterName.clear();
		valueParameterString.clear();
		valueParameterDouble = 0.0;
	}
	// RequestUpdatedID(behaviorID);
}

void RemoveStrip(AMFArrayValue* arguments) {
	StripId stripID = GetStripIDFromArgument(arguments);

	BehaviorState stateID = GetBehaviorStateFromArgument(arguments);

	uint32_t behaviorID = GetBehaviorIDFromArgument(arguments);

	// modelComponent->RemoveStrip(stateID, stripID, behaviorID);

	// RequestUpdatedID(behaviorID);
}

void MergeStrips(AMFArrayValue* arguments) {
	StripId srcStripID = GetStripIDFromArgument(arguments, "srcStripID");

	BehaviorState dstStateID = GetBehaviorStateFromArgument(arguments, "dstStateID");

	BehaviorState srcStateID = GetBehaviorStateFromArgument(arguments, "srcStateID");

	auto* dstActionIndexValue = arguments->FindValue<AMFDoubleValue>("dstActionIndex");
	if (!dstActionIndexValue) return;

	uint32_t dstActionIndex = static_cast<uint32_t>(dstActionIndexValue->GetDoubleValue());

	StripId dstStripID = GetStripIDFromArgument(arguments, "dstStripID");

	uint32_t behaviorID = GetBehaviorIDFromArgument(arguments);

	// modelComponent->MergeStrips(srcStripID, dstStripID, srcStateID, dstStateID, behaviorID, dstActionIndex);

	// RequestUpdatedID(behaviorID);
}

void SplitStrip(AMFArrayValue* arguments) {
	auto* srcActionIndexValue = arguments->FindValue<AMFDoubleValue>("srcActionIndex");
	if (!srcActionIndexValue) return;

	uint32_t srcActionIndex = static_cast<uint32_t>(srcActionIndexValue->GetDoubleValue());

	StripId srcStripID = GetStripIDFromArgument(arguments, "srcStripID");

	BehaviorState srcStateID = GetBehaviorStateFromArgument(arguments, "srcStateID");

	StripId dstStripID = GetStripIDFromArgument(arguments, "dstStripID");

	BehaviorState dstStateID = GetBehaviorStateFromArgument(arguments, "dstStateID");

	auto* dstStripUIArray = arguments->FindValue<AMFArrayValue>("dstStripUI");
	if (!dstStripUIArray) return;

	auto* xPositionValue = dstStripUIArray->FindValue<AMFDoubleValue>("x");
	auto* yPositionValue = dstStripUIArray->FindValue<AMFDoubleValue>("y");
	if (!xPositionValue || !yPositionValue) return;

	// x and y position 15 are just where the game puts the strip by default if none is given.
	double yPosition = yPositionValue->GetDoubleValue();
	double xPosition = xPositionValue->GetDoubleValue();

	uint32_t behaviorID = GetBehaviorIDFromArgument(arguments);

	// modelComponent->SplitStrip(srcActionIndex, srcStripID, srcStateID, dstStripID, dstStateID, behaviorID, yPosition, xPosition);

	// RequestUpdatedID(behaviorID);
}

void UpdateStripUI(AMFArrayValue* arguments) {
	auto* uiArray = arguments->FindValue<AMFArrayValue>("ui");
	if (!uiArray) return;

	auto* xPositionValue = uiArray->FindValue<AMFDoubleValue>("x");
	auto* yPositionValue = uiArray->FindValue<AMFDoubleValue>("y");
	if (!xPositionValue || !yPositionValue) return;

	double yPosition = yPositionValue->GetDoubleValue();
	double xPosition = xPositionValue->GetDoubleValue();

	StripId stripID = GetStripIDFromArgument(arguments);

	BehaviorState stateID = GetBehaviorStateFromArgument(arguments);

	uint32_t behaviorID = GetBehaviorIDFromArgument(arguments);

	// modelComponent->UpdateUIOfStrip(stateID, stripID, xPosition, yPosition, behaviorID);

	// RequestUpdatedID(behaviorID);
}

void AddAction(AMFArrayValue* arguments) {
	auto* actionIndexAmf = arguments->FindValue<AMFDoubleValue>("actionIndex");
	if (!actionIndexAmf) return;

	uint32_t actionIndex = static_cast<uint32_t>(actionIndexAmf->GetDoubleValue());

	StripId stripID = GetStripIDFromArgument(arguments);

	BehaviorState stateID = GetBehaviorStateFromArgument(arguments);

	std::string type = "";
	std::string valueParameterName = "";
	std::string valueParameterString = "";
	double valueParameterDouble = 0.0;
	auto* action = arguments->FindValue<AMFArrayValue>("action");
	if (!action) return;

	for (auto& typeValueMap : action->GetAssociativeMap()) {
		if (typeValueMap.first == "Type") {
			if (typeValueMap.second->GetValueType() != AMFValueType::AMFString) continue;
			type = static_cast<AMFStringValue*>(typeValueMap.second)->GetStringValue();
		} else {
			valueParameterName = typeValueMap.first;
			// Message is the only known string parameter
			if (valueParameterName == "Message") {
				if (typeValueMap.second->GetValueType() != AMFValueType::AMFString) continue;
				valueParameterString = static_cast<AMFStringValue*>(typeValueMap.second)->GetStringValue();
			} else {
				if (typeValueMap.second->GetValueType() != AMFValueType::AMFDouble) continue;
				valueParameterDouble = static_cast<AMFDoubleValue*>(typeValueMap.second)->GetDoubleValue();
			}
		}
	}

	uint32_t behaviorID = GetBehaviorIDFromArgument(arguments);

	// modelComponent->AddAction(stateID, stripID, type, valueParameterName, valueParameterString, valueParameterDouble, "", actionIndex, behaviorID);

	// RequestUpdatedID(behaviorID);
}

void MigrateActions(AMFArrayValue* arguments) {
	auto* srcActionIndexAmf = arguments->FindValue<AMFDoubleValue>("srcActionIndex");
	if (!srcActionIndexAmf) return;

	uint32_t srcActionIndex = static_cast<uint32_t>(srcActionIndexAmf->GetDoubleValue());

	StripId srcStripID = GetStripIDFromArgument(arguments, "srcStripID");

	BehaviorState srcStateID = GetBehaviorStateFromArgument(arguments, "srcStateID");

	auto* dstActionIndexAmf = arguments->FindValue<AMFDoubleValue>("dstActionIndex");
	if (!dstActionIndexAmf) return;

	uint32_t dstActionIndex = static_cast<uint32_t>(dstActionIndexAmf->GetDoubleValue());

	StripId dstStripID = GetStripIDFromArgument(arguments, "dstStripID");

	BehaviorState dstStateID = GetBehaviorStateFromArgument(arguments, "dstStateID");

	uint32_t behaviorID = GetBehaviorIDFromArgument(arguments);

	// modelComponent->MigrateActions(srcActionIndex, srcStripID, srcStateID, dstActionIndex, dstStripID, dstStateID, behaviorID);

	// RequestUpdatedID(behaviorID);
}

void RearrangeStrip(AMFArrayValue* arguments) {
	auto* srcActionIndexValue = arguments->FindValue<AMFDoubleValue>("srcActionIndex");
	uint32_t srcActionIndex = static_cast<uint32_t>(srcActionIndexValue->GetDoubleValue());

	uint32_t stripID = GetStripIDFromArgument(arguments);

	uint32_t behaviorID = GetBehaviorIDFromArgument(arguments);

	auto* dstActionIndexValue = arguments->FindValue<AMFDoubleValue>("dstActionIndex");
	uint32_t dstActionIndex = static_cast<uint32_t>(dstActionIndexValue->GetDoubleValue());

	BehaviorState stateID = GetBehaviorStateFromArgument(arguments);

	// modelComponent->RearrangeStrip(stateID, stripID, srcActionIndex, dstActionIndex, behaviorID);

	// RequestUpdatedID(behaviorID);
}

void Add(AMFArrayValue* arguments) {
	uint32_t behaviorID = GetBehaviorIDFromArgument(arguments);

	uint32_t behaviorIndex = 0;
	auto* behaviorIndexAmf = arguments->FindValue<AMFDoubleValue>("BehaviorIndex");

	if (!behaviorIndexAmf) return;

	behaviorIndex = static_cast<uint32_t>(behaviorIndexAmf->GetDoubleValue());

	// modelComponent->AddBehavior(behaviorID, behaviorIndex, modelOwner);

	// SendBehaviorListToClient();
}

void RemoveActions(AMFArrayValue* arguments) {
	uint32_t behaviorID = GetBehaviorIDFromArgument(arguments);

	auto* actionIndexAmf = arguments->FindValue<AMFDoubleValue>("actionIndex");
	if (!actionIndexAmf) return;

	uint32_t actionIndex = static_cast<uint32_t>(actionIndexAmf->GetDoubleValue());

	StripId stripID = GetStripIDFromArgument(arguments);

	BehaviorState stateID = GetBehaviorStateFromArgument(arguments);

	// modelComponent->RemoveAction(stateID, stripID, actionIndex, behaviorID);

	// RequestUpdatedID(behaviorID);
}

void Rename(Entity* modelEntity, const SystemAddress& sysAddr, Entity* modelOwner, AMFArrayValue* arguments) {
	uint32_t behaviorID = GetBehaviorIDFromArgument(arguments);

	auto* nameAmf = arguments->FindValue<AMFStringValue>("Name");
	if (!nameAmf) return;

	auto name = nameAmf->GetStringValue();

	// modelComponent->Rename(behaviorID, name);

	SendBehaviorListToClient(modelEntity, sysAddr, modelOwner);

	// RequestUpdatedID(behaviorID);
}

// TODO This is also supposed to serialize the state of the behaviors in progress but those aren't implemented yet
void SendBehaviorBlocksToClient(ModelComponent* modelComponent, const SystemAddress& sysAddr, Entity* modelOwner, AMFArrayValue* arguments) {
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

void UpdateAction(AMFArrayValue* arguments) {
	std::string type = "";
	std::string valueParameterName = "";
	std::string valueParameterString = "";
	double valueParameterDouble = 0.0;
	auto* actionAsArray = arguments->FindValue<AMFArrayValue>("action");
	if (!actionAsArray) return;
	for (auto& typeValueMap : actionAsArray->GetAssociativeMap()) {
		if (typeValueMap.first == "Type") {
			if (typeValueMap.second->GetValueType() != AMFValueType::AMFString) continue;
			type = static_cast<AMFStringValue*>(typeValueMap.second)->GetStringValue();
		} else {
			valueParameterName = typeValueMap.first;
			// Message is the only known string parameter
			if (valueParameterName == "Message") {
				if (typeValueMap.second->GetValueType() != AMFValueType::AMFString) continue;
				valueParameterString = static_cast<AMFStringValue*>(typeValueMap.second)->GetStringValue();
			} else {
				if (typeValueMap.second->GetValueType() != AMFValueType::AMFDouble) continue;
				valueParameterDouble = static_cast<AMFDoubleValue*>(typeValueMap.second)->GetDoubleValue();
			}
		}
	}

	uint32_t behaviorID = GetBehaviorIDFromArgument(arguments);

	auto* actionIndexValue = arguments->FindValue<AMFDoubleValue>("actionIndex");
	if (!actionIndexValue) return;

	uint32_t actionIndex = static_cast<uint32_t>(actionIndexValue->GetDoubleValue());

	StripId stripID = GetStripIDFromArgument(arguments);

	BehaviorState stateID = GetBehaviorStateFromArgument(arguments);

	// modelComponent->UpdateAction(stateID, stripID, type, valueParameterName, valueParameterString, valueParameterDouble, "", actionIndex, behaviorID);

	// RequestUpdatedID(behaviorID);
}

void MoveToInventory(ModelComponent* modelComponent, const SystemAddress& sysAddr, Entity* modelOwner, AMFArrayValue* arguments) {
	// This closes the UI menu should it be open while the player is removing behaviors
	AMFArrayValue args;

	AMFFalseValue* stateToPop = new AMFFalseValue();
	args.InsertValue("visible", stateToPop);

	GameMessages::SendUIMessageServerToSingleClient(modelOwner, modelOwner->GetParentUser()->GetSystemAddress(), "ToggleBehaviorEditor", &args);

	uint32_t behaviorID = GetBehaviorIDFromArgument(arguments);

	auto* behaviorIndexValue = arguments->FindValue<AMFDoubleValue>("BehaviorIndex");
	if (!behaviorIndexValue) return;

	uint32_t behaviorIndex = static_cast<uint32_t>(behaviorIndexValue->GetDoubleValue());

	// modelComponent->MoveBehaviorToInventory(behaviorID, behaviorIndex, modelOwner);

	SendBehaviorListToClient(modelComponent->GetParent(), sysAddr, modelOwner);
}

void ControlBehaviors::ProcessCommand(Entity* modelEntity, const SystemAddress& sysAddr, AMFArrayValue* arguments, std::string command, Entity* modelOwner) {
	if (!modelEntity || !modelOwner || !arguments) return;
	auto* modelComponent = modelEntity->GetComponent<ModelComponent>();

	if (!modelComponent) return;

	if (command == "sendBehaviorListToClient")
		SendBehaviorListToClient(modelEntity, sysAddr, modelOwner);
	else if (command == "modelTypeChanged")
		ModelTypeChanged(arguments, modelComponent);
	else if (command == "toggleExecutionUpdates")
		ToggleExecutionUpdates();
	else if (command == "addStrip")
		AddStrip(arguments);
	else if (command == "removeStrip")
		RemoveStrip(arguments);
	else if (command == "mergeStrips")
		MergeStrips(arguments);
	else if (command == "splitStrip")
		SplitStrip(arguments);
	else if (command == "updateStripUI")
		UpdateStripUI(arguments);
	else if (command == "addAction")
		AddAction(arguments);
	else if (command == "migrateActions")
		MigrateActions(arguments);
	else if (command == "rearrangeStrip")
		RearrangeStrip(arguments);
	else if (command == "add")
		Add(arguments);
	else if (command == "removeActions")
		RemoveActions(arguments);
	else if (command == "rename")
		Rename(modelEntity, sysAddr, modelOwner, arguments);
	else if (command == "sendBehaviorBlocksToClient")
		SendBehaviorBlocksToClient(modelComponent, sysAddr, modelOwner, arguments);
	else if (command == "moveToInventory")
		MoveToInventory(modelComponent, sysAddr, modelOwner, arguments);
	else if (command == "updateAction")
		UpdateAction(arguments);
	else
		Game::logger->Log("ControlBehaviors", "Unknown behavior command (%s)\n", command.c_str());
}
