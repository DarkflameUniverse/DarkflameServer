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
	constexpr static const eReplicaComponentType ComponentType = eReplicaComponentType::SCRIPT;

	ScriptComponent(const LWOOBJID& parent, const std::string& scriptName, const bool serialized, const bool client = false) noexcept;

	void Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate) override;

	/**
	 * Returns the script that's attached to this entity
	 * @return the script that's attached to this entity
	 */
	CppScripts::Script* GetScript();

	/**
	 * Sets whether the entity should be serialized, unused
	 * @param var whether the entity should be serialized
	 */
	void SetSerialized(const bool var) { m_Serialized = var; }

	/**
	 * Sets the script using a path by looking through dScripts for a script that matches
	 * @param scriptName the name of the script to find
	 */
	void SetScript(const std::string& scriptName);

private:

	/**
	 * The script attached to this entity
	 */
	CppScripts::Script* m_Script;

	/**
	 * Whether or not the comp should be serialized, unused
	 */
	bool m_Serialized;

	/**
	 * Whether or not this script is a client script
	 */
	bool m_Client;
};

#endif // SCRIPTCOMPONENT_H
