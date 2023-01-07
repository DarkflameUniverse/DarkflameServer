#pragma once

#ifndef __CONTROLBEHAVIORS__H__
#define __CONTROLBEHAVIORS__H__

#include <map>
#include <string>

#include "Singleton.h"

class BlockBase;
class Entity;
class AMFArrayValue;
class ModelComponent;

// Type definition to clarify what is used where
typedef std::string BlockName;					//! A block name

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
	BlockBase* GetBlockInfo(BlockName& blockName);
private:

	/**
	 * @brief Creates a block based on the given primitive type in typeName
	 * 
	 * @param typeName The primitive type to create the block based on.
	 * @return A pointer to a new block definition or a nullptr if the typing specified
	 * was not a valid typing for a block.
	 */
	BlockBase* CreateBehaviorBlock(std::string& name, std::string& typeName);
	std::map<BlockName, BlockBase*> blockTypes{};
};

#endif  //!__CONTROLBEHAVIORS__H__
