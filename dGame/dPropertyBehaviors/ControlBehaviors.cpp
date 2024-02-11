#include "ControlBehaviors.h"

#include "Amf3.h"
#include "Entity.h"
#include "Game.h"
#include "GameMessages.h"
#include "ModelComponent.h"
#include "ObjectIDManager.h"
#include "Logger.h"
#include "BehaviorStates.h"
#include "AssetManager.h"
#include "BlockDefinition.h"
#include "User.h"
#include "tinyxml2.h"
#include "CDClientDatabase.h"
#include "CharacterComponent.h"

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
	ObjectIDManager::RequestPersistentID(
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

// TODO This is also supposed to serialize the state of the behaviors in progress but those aren't implemented yet
void ControlBehaviors::SendBehaviorBlocksToClient(ControlBehaviorContext& context) {
	if (!context) return;
	BehaviorMessageBase behaviorMsg{ context.arguments };

	context.modelComponent->VerifyBehaviors();
	AMFArrayValue behavior;
	context.modelComponent->SendBehaviorBlocksToClient(behaviorMsg.GetBehaviorId(), behavior);
	GameMessages::SendUIMessageServerToSingleClient(context.modelOwner, context.modelOwner->GetSystemAddress(), "UpdateBehaviorBlocks", behavior);
}

void ControlBehaviors::UpdateAction(const AMFArrayValue& arguments) {
	UpdateActionMessage updateActionMessage{ arguments };
	auto blockDefinition = GetBlockInfo(updateActionMessage.GetAction().GetType());

	if (!blockDefinition) {
		LOG("Received undefined block type %s. Ignoring.", updateActionMessage.GetAction().GetType().data());
		return;
	}

	if (updateActionMessage.GetAction().GetValueParameterString().size() > 0) {
		if (updateActionMessage.GetAction().GetValueParameterString().size() < blockDefinition->GetMinimumValue() ||
			updateActionMessage.GetAction().GetValueParameterString().size() > blockDefinition->GetMaximumValue()) {
			LOG("Updated block %s is out of range. Ignoring update", updateActionMessage.GetAction().GetType().data());
			return;
		}
	} else {
		if (updateActionMessage.GetAction().GetValueParameterDouble() < blockDefinition->GetMinimumValue() ||
			updateActionMessage.GetAction().GetValueParameterDouble() > blockDefinition->GetMaximumValue()) {
			LOG("Updated block %s is out of range. Ignoring update", updateActionMessage.GetAction().GetType().data());
			return;
		}
	}
}

void ControlBehaviors::ProcessCommand(Entity* modelEntity, const AMFArrayValue& arguments, std::string_view command, Entity* modelOwner) {
	if (!isInitialized || !modelEntity || !modelOwner) return;
	auto* const modelComponent = modelEntity->GetComponent<ModelComponent>();

	if (!modelComponent) return;

	ControlBehaviorContext context{ arguments, modelComponent, modelOwner };

	if (command == "sendBehaviorListToClient") {
		SendBehaviorListToClient(context);
	} else if (command == "modelTypeChanged") {
		auto* const modelType = arguments.Get<double>("ModelType");
		if (!modelType) return;

		modelEntity->SetVar<int>(u"modelType", modelType->GetValue());
	} else if (command == "toggleExecutionUpdates") {
		// TODO
	} else if (command == "addStrip") {
		if (BehaviorMessageBase(context.arguments).IsDefaultBehaviorId()) RequestUpdatedID(context);

		context.modelComponent->HandleControlBehaviorsMsg<AddStripMessage>(context.arguments);
	} else if (command == "removeStrip") {
		context.modelComponent->HandleControlBehaviorsMsg<RemoveStripMessage>(arguments);
	} else if (command == "mergeStrips") {
		context.modelComponent->HandleControlBehaviorsMsg<MergeStripsMessage>(arguments);
	} else if (command == "splitStrip") {
		context.modelComponent->HandleControlBehaviorsMsg<SplitStripMessage>(arguments);
	} else if (command == "updateStripUI") {
		context.modelComponent->HandleControlBehaviorsMsg<UpdateStripUiMessage>(arguments);
	} else if (command == "addAction") {
		context.modelComponent->HandleControlBehaviorsMsg<AddActionMessage>(arguments);
	} else if (command == "migrateActions") {
		context.modelComponent->HandleControlBehaviorsMsg<MigrateActionsMessage>(arguments);
	} else if (command == "rearrangeStrip") {
		context.modelComponent->HandleControlBehaviorsMsg<RearrangeStripMessage>(arguments);
	} else if (command == "add") {
		AddMessage msg{ context.arguments };
		context.modelComponent->AddBehavior(msg);
		SendBehaviorListToClient(context);
	} else if (command == "removeActions") {
		context.modelComponent->HandleControlBehaviorsMsg<RemoveActionsMessage>(arguments);
	} else if (command == "rename") {
		context.modelComponent->HandleControlBehaviorsMsg<RenameMessage>(arguments);

		// Send the list back to the client so the name is updated.
		SendBehaviorListToClient(context);
	} else if (command == "sendBehaviorBlocksToClient") {
		SendBehaviorBlocksToClient(context);
	} else if (command == "moveToInventory") {
		MoveToInventoryMessage msg{ arguments };
		context.modelComponent->MoveToInventory(msg);
		auto* characterComponent = modelOwner->GetComponent<CharacterComponent>();
		if (!characterComponent) return;

		AMFArrayValue args;
		args.Insert("BehaviorID", std::to_string(msg.GetBehaviorId()));
		GameMessages::SendUIMessageServerToSingleClient(modelOwner, characterComponent->GetSystemAddress(), "BehaviorRemoved", args);

		SendBehaviorListToClient(context);
	} else if (command == "updateAction") {
		context.modelComponent->HandleControlBehaviorsMsg<UpdateActionMessage>(arguments);
	} else {
		LOG("Unknown behavior command (%s)", command.data());
	}
}

ControlBehaviors::ControlBehaviors() {
	auto blocksBuffer = Game::assetManager->GetFile("ui\\ingame\\blocksdef.xml");
	if (!blocksBuffer) {
		LOG("Failed to open blocksdef.xml, property behaviors will be disabled for this zone! "
			"(This is a necessary file for cheat detection and ensuring we do not send unexpected values to the client)");
		return;
	}

	tinyxml2::XMLDocument m_Doc;

	std::string read;

	std::string buffer;
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

			auto& blockDefinition = blockTypes[blockName];
			std::string name{};
			std::string typeName{};

			auto* argument = block->FirstChildElement("Argument");
			if (argument) {
				auto* defaultDefinition = argument->FirstChildElement("DefaultValue");
				if (defaultDefinition) blockDefinition.SetDefaultValue(defaultDefinition->GetText());

				auto* typeDefinition = argument->FirstChildElement("Type");
				if (typeDefinition) typeName = typeDefinition->GetText();

				auto* nameDefinition = argument->FirstChildElement("Name");
				if (nameDefinition) name = nameDefinition->GetText();

				// Now we parse the blocksdef file for the relevant information
				if (typeName == "String") {
					blockDefinition.SetMaximumValue(50); // The client has a hardcoded limit of 50 characters in a string field
				} else if (typeName == "Float" || typeName == "Integer") {
					auto* maximumDefinition = argument->FirstChildElement("Maximum");
					if (maximumDefinition) blockDefinition.SetMaximumValue(std::stof(maximumDefinition->GetText()));

					auto* minimumDefinition = argument->FirstChildElement("Minimum");
					if (minimumDefinition) blockDefinition.SetMinimumValue(std::stof(minimumDefinition->GetText()));
				} else if (typeName == "Enumeration") {
					auto* values = argument->FirstChildElement("Values");
					if (values) {
						auto* value = values->FirstChildElement("Value");
						while (value) {
							if (value->GetText() == blockDefinition.GetDefaultValue()) blockDefinition.GetDefaultValue() = std::to_string(blockDefinition.GetMaximumValue());
							blockDefinition.SetMaximumValue(blockDefinition.GetMaximumValue() + 1);
							value = value->NextSiblingElement("Value");
						}
						blockDefinition.SetMaximumValue(blockDefinition.GetMaximumValue() - 1); // Maximum value is 0 indexed
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
							blockDefinition.SetMaximumValue(res.getIntField("countSounds"));
							blockDefinition.SetDefaultValue("0");
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
			block = block->NextSiblingElement();
		}
		blockSections = blockSections->NextSiblingElement();
	}
	isInitialized = true;
	LOG_DEBUG("Created all base block classes");
	for (auto& [name, block] : blockTypes) {
		LOG_DEBUG("block name is %s default %s min %f max %f", name.data(), block.GetDefaultValue().data(), block.GetMinimumValue(), block.GetMaximumValue());
	}
}

std::optional<BlockDefinition> ControlBehaviors::GetBlockInfo(const std::string_view blockName) {
	auto blockDefinition = blockTypes.find(blockName);
	return blockDefinition != blockTypes.end() ? std::optional(blockDefinition->second) : std::nullopt;
}
