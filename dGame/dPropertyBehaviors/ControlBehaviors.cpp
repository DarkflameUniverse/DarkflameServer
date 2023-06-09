#include "ControlBehaviors.h"

#include "Amf3.h"
#include "Entity.h"
#include "Game.h"
#include "GameMessages.h"
#include "ModelBehaviorComponent.h"
#include "../../dWorldServer/ObjectIDManager.h"
#include "dLogger.h"
#include "BehaviorStates.h"
#include "AssetManager.h"
#include "BlockDefinition.h"
#include "User.h"
#include "tinyxml2.h"
#include "CDClientDatabase.h"

// Message includes
#include "Action.h"
#include "AddActionMessage.h"
#include "AddStripMessage.h"
#include "AddMessage.h"
#include "MigrateActionsMessage.h"
#include "MoveToInventoryMessage.h"
#include "MergeStripsMessage.h"
#include "RearrangeStripMessage.h"
#include "RemoveActionsMessage.h"
#include "RemoveStripMessage.h"
#include "RenameMessage.h"
#include "SplitStripMessage.h"
#include "UpdateActionMessage.h"
#include "UpdateStripUiMessage.h"

void ControlBehaviors::RequestUpdatedID(int32_t behaviorID, ModelBehaviorComponent* modelComponent, Entity* modelOwner, const SystemAddress& sysAddr) {
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
	// 		behaviorIDString->SetValue(std::to_string(persistentId));
	// 		args.InsertValue("behaviorID", behaviorIDString);

	// 		AMFStringValue* objectIDAsString = new AMFStringValue();
	// 		objectIDAsString->SetValue(std::to_string(modelComponent->GetOwningEntity()->GetObjectID()));
	// 		args.InsertValue("objectID", objectIDAsString);

	// 		GameMessages::SendUIMessageServerToSingleClient(modelOwner, sysAddr, "UpdateBehaviorID", &args);
	// 		ControlBehaviors::SendBehaviorListToClient(modelComponent->GetOwningEntity(), sysAddr, modelOwner);
	// 	});
	// }
}

void ControlBehaviors::SendBehaviorListToClient(Entity* modelEntity, const SystemAddress& sysAddr, Entity* modelOwner) {
	auto modelComponent = modelEntity->GetComponent<ModelBehaviorComponent>();

	if (!modelComponent) return;

	AMFArrayValue behaviorsToSerialize;

	/**
	 * The behaviors AMFArray will have up to 5 elements in the dense portion.
	 * Each element in the dense portion will be made up of another AMFArray
	 * with the following information mapped in the associative portion
	 * "id": Behavior ID cast to an AMFString
	 * "isLocked": AMFTrue or AMFFalse of whether or not the behavior is locked
	 * "isLoot": AMFTrue or AMFFalse of whether or not the behavior is a custom behavior (true if custom)
	 * "name": The name of the behavior formatted as an AMFString
	 */

	behaviorsToSerialize.Insert("behaviors");
	behaviorsToSerialize.Insert("objectID", std::to_string(modelComponent->GetOwningEntity()->GetObjectID()));

	GameMessages::SendUIMessageServerToSingleClient(modelOwner, sysAddr, "UpdateBehaviorList", behaviorsToSerialize);
}

void ControlBehaviors::ModelTypeChanged(AMFArrayValue* arguments, ModelBehaviorComponent* ModelComponent) {
	auto* modelTypeAmf = arguments->Get<double>("ModelType");
	if (!modelTypeAmf) return;

	uint32_t modelType = static_cast<uint32_t>(modelTypeAmf->GetValue());

	//TODO Update the model type here
}

void ControlBehaviors::ToggleExecutionUpdates() {
	//TODO do something with this info
}

void ControlBehaviors::AddStrip(AMFArrayValue* arguments) {
	AddStripMessage addStripMessage(arguments);
}

void ControlBehaviors::RemoveStrip(AMFArrayValue* arguments) {
	RemoveStripMessage removeStrip(arguments);
}

void ControlBehaviors::MergeStrips(AMFArrayValue* arguments) {
	MergeStripsMessage mergeStripsMessage(arguments);
}

void ControlBehaviors::SplitStrip(AMFArrayValue* arguments) {
	SplitStripMessage splitStripMessage(arguments);
}

void ControlBehaviors::UpdateStripUI(AMFArrayValue* arguments) {
	UpdateStripUiMessage updateStripUiMessage(arguments);
}

void ControlBehaviors::AddAction(AMFArrayValue* arguments) {
	AddActionMessage addActionMessage(arguments);
}

void ControlBehaviors::MigrateActions(AMFArrayValue* arguments) {
	MigrateActionsMessage migrateActionsMessage(arguments);
}

void ControlBehaviors::RearrangeStrip(AMFArrayValue* arguments) {
	RearrangeStripMessage rearrangeStripMessage(arguments);
}

void ControlBehaviors::Add(AMFArrayValue* arguments) {
	AddMessage addMessage(arguments);
}

void ControlBehaviors::RemoveActions(AMFArrayValue* arguments) {
	RemoveActionsMessage removeActionsMessage(arguments);
}

void ControlBehaviors::Rename(Entity* modelEntity, const SystemAddress& sysAddr, Entity* modelOwner, AMFArrayValue* arguments) {
	RenameMessage renameMessage(arguments);
}

// TODO This is also supposed to serialize the state of the behaviors in progress but those aren't implemented yet
void ControlBehaviors::SendBehaviorBlocksToClient(ModelBehaviorComponent* modelComponent, const SystemAddress& sysAddr, Entity* modelOwner, AMFArrayValue* arguments) {
	// uint32_t behaviorID = ControlBehaviors::GetBehaviorIDFromArgument(arguments);

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
	// 	stateAsDouble->SetValue(it->first);
	// 	state->InsertValue("id", stateAsDouble);

	// 	AMFArrayValue* strips = new AMFArrayValue();
	// 	auto stripsInState = it->second->GetStrips();
	// 	for (auto strip = stripsInState.begin(); strip != stripsInState.end(); strip++) {
	// 		Game::logger->Log("PropertyBehaviors", "Begin serialization of strip %i!\n", strip->first);
	// 		AMFArrayValue* thisStrip = new AMFArrayValue();

	// 		AMFDoubleValue* stripID = new AMFDoubleValue();
	// 		stripID->SetValue(strip->first);
	// 		thisStrip->InsertValue("id", stripID);

	// 		AMFArrayValue* uiArray = new AMFArrayValue();
	// 		AMFDoubleValue* yPosition = new AMFDoubleValue();
	// 		yPosition->SetValue(strip->second->GetYPosition());
	// 		uiArray->InsertValue("y", yPosition);

	// 		AMFDoubleValue* xPosition = new AMFDoubleValue();
	// 		xPosition->SetValue(strip->second->GetXPosition());
	// 		uiArray->InsertValue("x", xPosition);

	// 		thisStrip->InsertValue("ui", uiArray);
	// 		targetObjectID = modelComponent->GetOwningEntity()->GetObjectID();
	// 		behaviorID = modelBehavior->GetBehaviorID();

	// 		AMFArrayValue* stripSerialize = new AMFArrayValue();
	// 		for (auto behaviorAction : strip->second->GetActions()) {
	// 			Game::logger->Log("PropertyBehaviors", "Begin serialization of action %s!\n", behaviorAction->actionName.c_str());
	// 			AMFArrayValue* thisAction = new AMFArrayValue();

	// 			AMFStringValue* actionName = new AMFStringValue();
	// 			actionName->SetValue(behaviorAction->actionName);
	// 			thisAction->InsertValue("Type", actionName);

	// 			if (behaviorAction->parameterValueString != "")
	// 			{
	// 				AMFStringValue* valueAsString = new AMFStringValue();
	// 				valueAsString->SetValue(behaviorAction->parameterValueString);
	// 				thisAction->InsertValue(behaviorAction->parameterName, valueAsString);
	// 			}
	// 			else if (behaviorAction->parameterValueDouble != 0.0)
	// 			{
	// 				AMFDoubleValue* valueAsDouble = new AMFDoubleValue();
	// 				valueAsDouble->SetValue(behaviorAction->parameterValueDouble);
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
	// objectidAsString->SetValue(std::to_string(targetObjectID));
	// behaviorInfo.InsertValue("objectID", objectidAsString);

	// AMFStringValue* behaviorIDAsString = new AMFStringValue();
	// behaviorIDAsString->SetValue(std::to_string(behaviorID));
	// behaviorInfo.InsertValue("BehaviorID", behaviorIDAsString);

	// GameMessages::SendUIMessageServerToSingleClient(modelOwner, sysAddr, "UpdateBehaviorBlocks", &behaviorInfo);
}

void ControlBehaviors::UpdateAction(AMFArrayValue* arguments) {
	UpdateActionMessage updateActionMessage(arguments);
	auto* blockDefinition = GetBlockInfo(updateActionMessage.GetAction().GetType());

	if (!blockDefinition) {
		Game::logger->Log("ControlBehaviors", "Received undefined block type %s. Ignoring.", updateActionMessage.GetAction().GetType().c_str());
		return;
	}

	if (updateActionMessage.GetAction().GetValueParameterString().size() > 0) {
		if (updateActionMessage.GetAction().GetValueParameterString().size() < blockDefinition->GetMinimumValue() ||
			updateActionMessage.GetAction().GetValueParameterString().size() > blockDefinition->GetMaximumValue()) {
			Game::logger->Log("ControlBehaviors", "Updated block %s is out of range. Ignoring update", updateActionMessage.GetAction().GetType().c_str());
			return;
		}
	} else {
		if (updateActionMessage.GetAction().GetValueParameterDouble() < blockDefinition->GetMinimumValue() ||
			updateActionMessage.GetAction().GetValueParameterDouble() > blockDefinition->GetMaximumValue()) {
			Game::logger->Log("ControlBehaviors", "Updated block %s is out of range. Ignoring update", updateActionMessage.GetAction().GetType().c_str());
			return;
		}
	}
}

void ControlBehaviors::MoveToInventory(ModelBehaviorComponent* modelComponent, const SystemAddress& sysAddr, Entity* modelOwner, AMFArrayValue* arguments) {
	// This closes the UI menu should it be open while the player is removing behaviors
	AMFArrayValue args;

	args.Insert("visible", false);

	GameMessages::SendUIMessageServerToSingleClient(modelOwner, modelOwner->GetParentUser()->GetSystemAddress(), "ToggleBehaviorEditor", args);

	MoveToInventoryMessage moveToInventoryMessage(arguments);

	SendBehaviorListToClient(modelComponent->GetOwningEntity(), sysAddr, modelOwner);
}

void ControlBehaviors::ProcessCommand(Entity* modelEntity, const SystemAddress& sysAddr, AMFArrayValue* arguments, std::string command, Entity* modelOwner) {
	if (!isInitialized || !modelEntity || !modelOwner || !arguments) return;
	auto modelComponent = modelEntity->GetComponent<ModelBehaviorComponent>();

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

ControlBehaviors::ControlBehaviors() {
	auto blocksDefStreamBuffer = Game::assetManager->GetFileAsBuffer("ui\\ingame\\blocksdef.xml");
	if (!blocksDefStreamBuffer.m_Success) {
		Game::logger->Log("ControlBehaviors", "failed to open blocksdef");
		return;
	}
	std::istream blocksBuffer(&blocksDefStreamBuffer);
	if (!blocksBuffer.good()) {
		Game::logger->Log("ControlBehaviors", "Blocks buffer is not good!");
		return;
	}

	tinyxml2::XMLDocument m_Doc;

	std::string read{};

	std::string buffer{};
	bool commentBlockStart = false;
	while (std::getline(blocksBuffer, read)) {
		// tinyxml2 should handle comment blocks but the client has one that fails the processing.
		// This preprocessing just removes all comments from the read file out of an abundance of caution.
		if (read.find("<!--") != std::string::npos) {
			commentBlockStart = true;
		}
		if (read.find("-->") != std::string::npos) {
			commentBlockStart = false;
			continue;
		}
		if (!commentBlockStart) buffer += read;
	}

	auto ret = m_Doc.Parse(buffer.c_str());
	if (ret == tinyxml2::XML_SUCCESS) {
		Game::logger->LogDebug("ControlBehaviors", "Successfully parsed the blocksdef file!");
	} else {
		Game::logger->Log("Character", "Failed to parse BlocksDef xmlData due to error %i!", ret);
		return;
	}
	auto* blockLibrary = m_Doc.FirstChildElement();
	if (!blockLibrary) {
		Game::logger->Log("ControlBehaviors", "No Block Library child element found.");
		return;
	}

	// Now parse the blocksdef for the cheat detection server side.
	// The client does these checks, but a bad actor can bypass the client checks
	auto* blockSections = blockLibrary->FirstChildElement();
	while (blockSections) {
		auto* block = blockSections->FirstChildElement();
		std::string blockName{};
		while (block) {
			blockName = block->Name();

			BlockDefinition* blockDefinition = new BlockDefinition();
			std::string name{};
			std::string typeName{};

			auto* argument = block->FirstChildElement("Argument");
			if (argument) {
				auto* defaultDefinition = argument->FirstChildElement("DefaultValue");
				if (defaultDefinition) blockDefinition->SetDefaultValue(defaultDefinition->GetText());

				auto* typeDefinition = argument->FirstChildElement("Type");
				if (typeDefinition) typeName = typeDefinition->GetText();

				auto* nameDefinition = argument->FirstChildElement("Name");
				if (nameDefinition) name = nameDefinition->GetText();

				// Now we parse the blocksdef file for the relevant information
				if (typeName == "String") {
					blockDefinition->SetMaximumValue(50); // The client has a hardcoded limit of 50 characters in a string field
				} else if (typeName == "Float" || typeName == "Integer") {
					auto* maximumDefinition = argument->FirstChildElement("Maximum");
					if (maximumDefinition) blockDefinition->SetMaximumValue(std::stof(maximumDefinition->GetText()));

					auto* minimumDefinition = argument->FirstChildElement("Minimum");
					if (minimumDefinition) blockDefinition->SetMinimumValue(std::stof(minimumDefinition->GetText()));
				} else if (typeName == "Enumeration") {
					auto* values = argument->FirstChildElement("Values");
					if (values) {
						auto* value = values->FirstChildElement("Value");
						while (value) {
							if (value->GetText() == blockDefinition->GetDefaultValue()) blockDefinition->GetDefaultValue() = std::to_string(blockDefinition->GetMaximumValue());
							blockDefinition->SetMaximumValue(blockDefinition->GetMaximumValue() + 1);
							value = value->NextSiblingElement("Value");
						}
						blockDefinition->SetMaximumValue(blockDefinition->GetMaximumValue() - 1); // Maximum value is 0 indexed
					} else {
						values = argument->FirstChildElement("EnumerationSource");
						if (!values) {
							Game::logger->Log("ControlBehaviors", "Failed to parse EnumerationSource from block (%s)", blockName.c_str());
							continue;
						}

						auto* serviceNameNode = values->FirstChildElement("ServiceName");
						if (!serviceNameNode) {
							Game::logger->Log("ControlBehaviors", "Failed to parse ServiceName from block (%s)", blockName.c_str());
							continue;
						}

						std::string serviceName = serviceNameNode->GetText();
						if (serviceName == "GetBehaviorSoundList") {
							auto res = CDClientDatabase::ExecuteQuery("SELECT MAX(id) as countSounds FROM UGBehaviorSounds;");
							blockDefinition->SetMaximumValue(res.getIntField("countSounds"));
							blockDefinition->SetDefaultValue("0");
						} else {
							Game::logger->Log("ControlBehaviors", "Unsupported Enumeration ServiceType (%s)", serviceName.c_str());
							continue;
						}
					}
				} else {
					Game::logger->Log("ControlBehaviors", "Unsupported block value type (%s)!", typeName.c_str());
					continue;
				}
			}
			blockTypes.insert(std::make_pair(blockName, blockDefinition));
			block = block->NextSiblingElement();
		}
		blockSections = blockSections->NextSiblingElement();
	}
	isInitialized = true;
	Game::logger->LogDebug("ControlBehaviors", "Created all base block classes");
	for (auto b : blockTypes) {
		Game::logger->LogDebug("ControlBehaviors", "block name is %s default %s min %f max %f", b.first.c_str(), b.second->GetDefaultValue().c_str(), b.second->GetMinimumValue(), b.second->GetMaximumValue());
	}
}

BlockDefinition* ControlBehaviors::GetBlockInfo(const BlockName& blockName) {
	auto blockDefinition = blockTypes.find(blockName);
	return blockDefinition != blockTypes.end() ? blockDefinition->second : nullptr;
}
