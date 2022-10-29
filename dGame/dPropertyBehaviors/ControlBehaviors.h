#pragma once

#ifndef __CONTROLBEHAVIORS__H__
#define __CONTROLBEHAVIORS__H__

#include <string>

#include "RakNetTypes.h"

class Entity;
class AMFArrayValue;
class ModelComponent;

namespace ControlBehaviors {
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
	 * @brief Helper function to send the behavior list to the client
	 *
	 * @param modelEntity The model that sent this command
	 * @param sysAddr The SystemAddress to respond to
	 * @param modelOwner The owner of the model which sent this command
	 */
	void SendBehaviorListToClient(Entity* modelEntity, const SystemAddress& sysAddr, Entity* modelOwner);

	/**
	 * Changes the model type of the model
	 */
	void ModelTypeChanged(AMFArrayValue* arguments, ModelComponent* ModelComponent);

	/**
	 * Unknown use at the moment.
	 * Only sent when the player edits a behavior (client sends true)
	 * or closes the UI (client sends false).
	 */
	void ToggleExecutionUpdates();

	/**
	 * Adds a strip to a behavior
	 */
	void AddStrip(AMFArrayValue* arguments);

	/**
	 * Removes a strip from a behavior
	 */
	void RemoveStrip(AMFArrayValue* arguments);

	/**
	 * Merges two strips into one
	 */
	void MergeStrips(AMFArrayValue* arguments);

	/**
	 * Splits a strip into two strips
	 */
	void SplitStrip(AMFArrayValue* arguments);

	/**
	 * Updates the UI position of a strip
	 */
	void UpdateStripUI(AMFArrayValue* arguments);

	/**
	 * Adds an action to a strip
	 */
	void AddAction(AMFArrayValue* arguments);

	/**
	 * Migrates actions from one strip to another
	 */
	void MigrateActions(AMFArrayValue* arguments);

	/**
	 * Rearranges the order of actions within a strip
	 */
	void RearrangeStrip(AMFArrayValue* arguments);

	/**
	 * Adds a behavior to a model
	 */
	void Add(AMFArrayValue* arguments);

	/**
	 * Removes any number of actions (1 or more) from a strip
	 */
	void RemoveActions(AMFArrayValue* arguments);

	/**
	 * Renames a behavior
	 */
	void Rename(Entity* modelEntity, const SystemAddress& sysAddr, Entity* modelOwner, AMFArrayValue* arguments);

	/**
	 * Sends the behavior blocks of a behavior to the client
	 */
	void SendBehaviorBlocksToClient(ModelComponent* modelComponent, const SystemAddress& sysAddr, Entity* modelOwner, AMFArrayValue* arguments);

	/**
	 * Updates the parameters of an action
	 */
	void UpdateAction(AMFArrayValue* arguments);

	/**
	 * Moves a behavior into the players inventory
	 */
	void MoveToInventory(ModelComponent* modelComponent, const SystemAddress& sysAddr, Entity* modelOwner, AMFArrayValue* arguments);
};

#endif  //!__CONTROLBEHAVIORS__H__
