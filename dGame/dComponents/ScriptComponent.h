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
class ScriptComponent final : public Component {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::SCRIPT;

	ScriptComponent(Entity* parent, const int32_t componentID, const std::string& scriptName, bool serialized, bool client = false);

	void Serialize(RakNet::BitStream& outBitStream, bool bIsInitialUpdate) override;

	/**
	 * Returns the script that's attached to this entity
	 * @return the script that's attached to this entity
	 */
	CppScripts::Script* const GetScript();

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
	
	bool OnGetObjectReportInfo(GameMessages::GetObjectReportInfo& reportInfo);

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

	std::string m_ScriptName;
};

#endif // SCRIPTCOMPONENT_H
