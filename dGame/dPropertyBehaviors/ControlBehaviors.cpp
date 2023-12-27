#include "ControlBehaviors.h"

#include "Amf3.h"
#include "Entity.h"
#include "Game.h"
#include "GameMessages.h"
#include "ModelComponent.h"
#include "../../dWorldServer/ObjectIDManager.h"
#include "Logger.h"
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

void ControlBehaviors::RequestUpdatedID(ControlBehaviorContext& context) {
	ObjectIDManager::Instance()->RequestPersistentID(
		[context](uint32_t persistentId) {
			if (!context) {
				LOG("Model to update behavior ID for is null. Cannot update ID.");
				return;
			}
			// This updates the behavior ID of the behavior should this be a new behavior
			AMFArrayValue args;

			args.Insert("behaviorID", std::to_string(persistentId));
			args.Insert("objectID", std::to_string(context.modelComponent->GetParent()->GetObjectID()));

			GameMessages::SendUIMessageServerToSingleClient(context.modelOwner, context.modelOwner->GetSystemAddress(), "UpdateBehaviorID", args);
			context.modelComponent->UpdatePendingBehaviorId(persistentId);

			ControlBehaviors::Instance().SendBehaviorListToClient(context);
		});
}

void ControlBehaviors::SendBehaviorListToClient(const ControlBehaviorContext& context) {
	if (!context) return;

	AMFArrayValue behaviorsToSerialize;
	context.modelComponent->SendBehaviorListToClient(behaviorsToSerialize);

	GameMessages::SendUIMessageServerToSingleClient(context.modelOwner, context.modelOwner->GetSystemAddress(), "UpdateBehaviorList", behaviorsToSerialize);
}

void ControlBehaviors::ModelTypeChanged(AMFArrayValue* arguments, ModelComponent* ModelComponent) {
	auto* modelTypeAmf = arguments->Get<double>("ModelType");
	if (!modelTypeAmf) return;

	uint32_t modelType = static_cast<uint32_t>(modelTypeAmf->GetValue());

	//TODO Update the model type here
}

void ControlBehaviors::ToggleExecutionUpdates() {
	//TODO do something with this info
}

void ControlBehaviors::AddStrip(ControlBehaviorContext& context) {
	AddStripMessage addStripMessage(context.arguments);

	if (addStripMessage.IsDefaultBehaviorId()) {
		RequestUpdatedID(context);
	}

	context.modelComponent->HandleControlBehaviorsMsg(addStripMessage);
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
void ControlBehaviors::SendBehaviorBlocksToClient(ControlBehaviorContext& context) {
	BehaviorMessageBase behaviorMsg(context.arguments);

	AMFArrayValue behavior;
	behavior.Insert("BehaviorID", std::to_string(behaviorMsg.GetBehaviorId()));
	behavior.Insert("objectID", std::to_string(context.modelComponent->GetParent()->GetObjectID()));
	auto* stateArray = behavior.InsertArray("states");
	for (const auto& [stateId, state] : context.modelComponent->m_Behaviors[behaviorMsg.GetBehaviorId()].m_States) {
		auto* stateInfo = stateArray->PushArray();
		stateInfo->Insert("id", static_cast<double>(stateId));

		auto* stripArray = stateInfo->InsertArray("strips");
		for (int32_t stripId = 0; stripId < state.m_Strips.size(); stripId++) {
			auto strip = state.m_Strips.at(stripId);

			auto* stripInfo = stripArray->InsertArray(stripId);
			stripInfo->Insert("id", static_cast<double>(stripId));
			auto* uiInfo = stripInfo->InsertArray("ui");
			uiInfo->Insert("x", strip.m_Position.GetX());
			uiInfo->Insert("y", strip.m_Position.GetY());

			auto* actions = stripInfo->InsertArray("actions");
			for (int32_t i = 0; i < strip.m_Actions.size(); i++) {
				auto action = strip.m_Actions.at(i);
				auto* actionInfo = actions->InsertArray(i);
				actionInfo->Insert("Type", action.GetType());
				if (action.GetValueParameterName() == "Message") {
					actionInfo->Insert(action.GetValueParameterName(), action.GetValueParameterString());
				} else if (!action.GetValueParameterName().empty()) {
					actionInfo->Insert(action.GetValueParameterName(), action.GetValueParameterDouble());
				}
			}
		}
	}

	GameMessages::SendUIMessageServerToSingleClient(context.modelOwner, context.modelOwner->GetSystemAddress(), "UpdateBehaviorBlocks", behavior);
}

void ControlBehaviors::UpdateAction(AMFArrayValue* arguments) {
	UpdateActionMessage updateActionMessage(arguments);
	auto* blockDefinition = GetBlockInfo(updateActionMessage.GetAction().GetType());

	if (!blockDefinition) {
		LOG("Received undefined block type %s. Ignoring.", updateActionMessage.GetAction().GetType().c_str());
		return;
	}

	if (updateActionMessage.GetAction().GetValueParameterString().size() > 0) {
		if (updateActionMessage.GetAction().GetValueParameterString().size() < blockDefinition->GetMinimumValue() ||
			updateActionMessage.GetAction().GetValueParameterString().size() > blockDefinition->GetMaximumValue()) {
			LOG("Updated block %s is out of range. Ignoring update", updateActionMessage.GetAction().GetType().c_str());
			return;
		}
	} else {
		if (updateActionMessage.GetAction().GetValueParameterDouble() < blockDefinition->GetMinimumValue() ||
			updateActionMessage.GetAction().GetValueParameterDouble() > blockDefinition->GetMaximumValue()) {
			LOG("Updated block %s is out of range. Ignoring update", updateActionMessage.GetAction().GetType().c_str());
			return;
		}
	}
}

void ControlBehaviors::MoveToInventory(ModelComponent* modelComponent, const SystemAddress& sysAddr, Entity* modelOwner, AMFArrayValue* arguments) {
	// This closes the UI menu should it be open while the player is removing behaviors
	AMFArrayValue args;

	args.Insert("visible", false);

	GameMessages::SendUIMessageServerToSingleClient(modelOwner, modelOwner->GetParentUser()->GetSystemAddress(), "ToggleBehaviorEditor", args);

	MoveToInventoryMessage moveToInventoryMessage(arguments);

	// SendBehaviorListToClient(modelComponent->GetParent(), sysAddr, modelOwner);
}

void ControlBehaviors::ProcessCommand(Entity* modelEntity, const SystemAddress& sysAddr, AMFArrayValue* arguments, std::string command, Entity* modelOwner) {
	if (!isInitialized || !modelEntity || !modelOwner || !arguments) return;
	auto* modelComponent = modelEntity->GetComponent<ModelComponent>();

	if (!modelComponent) return;

	ControlBehaviorContext context(arguments, modelComponent, modelOwner);

	if (command == "sendBehaviorListToClient") {
		SendBehaviorListToClient(context);
	} else if (command == "modelTypeChanged") {
		ModelTypeChanged(arguments, modelComponent);
	} else if (command == "toggleExecutionUpdates") {
		ToggleExecutionUpdates();
	} else if (command == "addStrip") {
		AddStrip(context);
	} else if (command == "removeStrip") {
		RemoveStrip(arguments);
	} else if (command == "mergeStrips") {
		MergeStrips(arguments);
	} else if (command == "splitStrip") {
		SplitStrip(arguments);
	} else if (command == "updateStripUI") {
		UpdateStripUI(arguments);
	} else if (command == "addAction") {
		AddAction(arguments);
	} else if (command == "migrateActions") {
		MigrateActions(arguments);
	} else if (command == "rearrangeStrip") {
		RearrangeStrip(arguments);
	} else if (command == "add") {
		Add(arguments);
	} else if (command == "removeActions") {
		RemoveActions(arguments);
	} else if (command == "rename") {
		Rename(modelEntity, sysAddr, modelOwner, arguments);
	} else if (command == "sendBehaviorBlocksToClient") {
		SendBehaviorBlocksToClient(context);
	} else if (command == "moveToInventory") {
		MoveToInventory(modelComponent, sysAddr, modelOwner, arguments);
	} else if (command == "updateAction") {
		UpdateAction(arguments);
	} else {
		LOG("Unknown behavior command (%s)", command.c_str());
	}
}

ControlBehaviors::ControlBehaviors() {
	auto blocksBuffer = Game::assetManager->GetFile("ui\\ingame\\blocksdef.xml");
	if (!blocksBuffer) {
		LOG("Failed to open blocksdef.xml");
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
		LOG_DEBUG("Successfully parsed the blocksdef file!");
	} else {
		LOG("Failed to parse BlocksDef xmlData due to error %i!", ret);
		return;
	}
	auto* blockLibrary = m_Doc.FirstChildElement();
	if (!blockLibrary) {
		LOG("No Block Library child element found.");
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
							LOG("Failed to parse EnumerationSource from block (%s)", blockName.c_str());
							continue;
						}

						auto* serviceNameNode = values->FirstChildElement("ServiceName");
						if (!serviceNameNode) {
							LOG("Failed to parse ServiceName from block (%s)", blockName.c_str());
							continue;
						}

						std::string serviceName = serviceNameNode->GetText();
						if (serviceName == "GetBehaviorSoundList") {
							auto res = CDClientDatabase::ExecuteQuery("SELECT MAX(id) as countSounds FROM UGBehaviorSounds;");
							blockDefinition->SetMaximumValue(res.getIntField("countSounds"));
							blockDefinition->SetDefaultValue("0");
						} else {
							LOG("Unsupported Enumeration ServiceType (%s)", serviceName.c_str());
							continue;
						}
					}
				} else {
					LOG("Unsupported block value type (%s)!", typeName.c_str());
					continue;
				}
			}
			blockTypes.insert(std::make_pair(blockName, blockDefinition));
			block = block->NextSiblingElement();
		}
		blockSections = blockSections->NextSiblingElement();
	}
	isInitialized = true;
	LOG_DEBUG("Created all base block classes");
	for (auto b : blockTypes) {
		LOG_DEBUG("block name is %s default %s min %f max %f", b.first.c_str(), b.second->GetDefaultValue().c_str(), b.second->GetMinimumValue(), b.second->GetMaximumValue());
	}
}

BlockDefinition* ControlBehaviors::GetBlockInfo(const BlockName& blockName) {
	auto blockDefinition = blockTypes.find(blockName);
	return blockDefinition != blockTypes.end() ? blockDefinition->second : nullptr;
}
