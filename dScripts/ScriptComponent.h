/*
 * Darkflame Universe
 * Copyright 2018
 */

#ifndef SCRIPTCOMPONENT_H
#define SCRIPTCOMPONENT_H

#include "CppScripts.h"
#include "Component.h"
#include <string>
#include "eReplicaComponentType.h"

class Entity;

/**
 * Handles the loading and execution of server side scripts on entities, scripts were originally written in Lua,
 * here they're written in C++
 */
class ScriptComponent : public Component {
public:
	inline static const eReplicaComponentType ComponentType = eReplicaComponentType::SCRIPT;

	ScriptComponent(Entity* parent, std::string scriptName);

	void Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags);

	/**
	 * Returns the script that's attached to this entity
	 * @return the script that's attached to this entity
	 */
	CppScripts::Script* GetScript();

	/**
	 * Sets the script using a path by looking through dScripts for a script that matches
	 * @param scriptName the name of the script to find
	 */
	void SetScript(const std::string& scriptName);

	// Get the script attached to the provided component id.
	// LDF key custom_script_server overrides component id script.
	static const std::string GetScriptName(Entity* parentEntity, const uint32_t componentId = 0);

	static const std::string GetZoneScriptName(const uint32_t componentId);
private:

	/**
	 * The script attached to this entity
	 */
	CppScripts::Script* m_Script;
};

#endif // SCRIPTCOMPONENT_H
