#pragma once

#ifndef __CONTROLBEHAVIORS__H__
#define __CONTROLBEHAVIORS__H__

#include <map>
#include <string>

#include "Singleton.h"

class Entity;
class AMFArrayValue;
class ModelComponent;

class ControlBehaviors: public Singleton<ControlBehaviors> {
public:
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
	ControlBehaviors();

private:
	std::map<std::string, std::string> blockTypes{};
};

#endif  //!__CONTROLBEHAVIORS__H__
