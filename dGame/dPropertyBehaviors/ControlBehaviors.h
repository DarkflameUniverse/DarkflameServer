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
};

#endif  //!__CONTROLBEHAVIORS__H__
