#pragma once

#ifndef __CONTROLBEHAVIORS__H__
#define __CONTROLBEHAVIORS__H__

#include <map>
#include <optional>
#include <string>

#include "BlockDefinition.h"
#include "Singleton.h"

class AMFArrayValue;
class Entity;
class ModelComponent;
class SystemAddress;

// Type definition to clarify what is used where
typedef std::string BlockName;					//! A block name

struct ControlBehaviorContext {
	ControlBehaviorContext(AMFArrayValue* args, ModelComponent* modelComponent, Entity* modelOwner) : arguments(args), modelComponent(modelComponent), modelOwner(modelOwner) {};

	operator bool() const {
		return arguments != nullptr && modelComponent != nullptr && modelOwner != nullptr;
	}

	AMFArrayValue* arguments;
	Entity* modelOwner;
	ModelComponent* modelComponent;
};

class ControlBehaviors: public Singleton<ControlBehaviors> {
public:
	ControlBehaviors();
	/**
	 * @brief Main driver for processing Property Behavior commands
	 *
	 * @param modelEntity The model that sent this command
	 * @param sysAddr The SystemAddress to respond to
	 * @param arguments The arguments formatted as an AMFArrayValue
	 * @param command The command to perform
	 * @param modelOwner The owner of the model which sent this command
	 */
	void ProcessCommand(Entity* modelEntity, const SystemAddress& sysAddr, AMFArrayValue* arguments, std::string command, Entity* modelOwner);

	/**
	 * @brief Gets a blocks parameter values by the name
	 * No exception will be thrown in this function.
	 * 
	 * @param blockName The block name to get the parameters of
	 * 
	 * @return A pair of the block parameter name to its typing
	 */
	std::optional<BlockDefinition> GetBlockInfo(const BlockName& blockName);
private:
	void RequestUpdatedID(ControlBehaviorContext& context);
	void SendBehaviorListToClient(const ControlBehaviorContext& context);
	void SendBehaviorBlocksToClient(ControlBehaviorContext& context);
	void UpdateAction(AMFArrayValue* arguments);
	std::map<BlockName, BlockDefinition> blockTypes{};

	// If false, property behaviors will not be able to be edited.
	bool isInitialized = false;
};

#endif  //!__CONTROLBEHAVIORS__H__
