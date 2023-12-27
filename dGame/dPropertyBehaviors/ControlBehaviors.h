#pragma once

#ifndef __CONTROLBEHAVIORS__H__
#define __CONTROLBEHAVIORS__H__

#include <map>
#include <string>

#include "Singleton.h"

class AMFArrayValue;
class BlockDefinition;
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
	BlockDefinition* GetBlockInfo(const BlockName& blockName);
private:
	void RequestUpdatedID(ControlBehaviorContext& context);
	void SendBehaviorListToClient(const ControlBehaviorContext& context);
	void ModelTypeChanged(AMFArrayValue* arguments, ModelComponent* ModelComponent);
	void ToggleExecutionUpdates();
	void AddStrip(ControlBehaviorContext& context);
	void RemoveStrip(AMFArrayValue* arguments);
	void MergeStrips(AMFArrayValue* arguments);
	void SplitStrip(AMFArrayValue* arguments);
	void UpdateStripUI(AMFArrayValue* arguments);
	void AddAction(AMFArrayValue* arguments);
	void MigrateActions(AMFArrayValue* arguments);
	void RearrangeStrip(AMFArrayValue* arguments);
	void Add(AMFArrayValue* arguments);
	void RemoveActions(AMFArrayValue* arguments);
	void Rename(Entity* modelEntity, const SystemAddress& sysAddr, Entity* modelOwner, AMFArrayValue* arguments);
	void SendBehaviorBlocksToClient(ControlBehaviorContext& context);
	void UpdateAction(AMFArrayValue* arguments);
	void MoveToInventory(ModelComponent* modelComponent, const SystemAddress& sysAddr, Entity* modelOwner, AMFArrayValue* arguments);
	std::map<BlockName, BlockDefinition*> blockTypes{};

	// If false, property behaviors will not be able to be edited.
	bool isInitialized = false;
};

#endif  //!__CONTROLBEHAVIORS__H__
