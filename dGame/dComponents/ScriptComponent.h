/*
 * Darkflame Universe
 * Copyright 2018
 */

#ifndef SCRIPTCOMPONENT_H
#define SCRIPTCOMPONENT_H

#include "CppScripts.h"
#include "Component.h"
#include "GameMessages.h"
#include <functional>
#include <map>
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

	// Registers a message from a script to be listened for on the parent object
	template<typename ScriptClass, typename DerivedMsgType>
	void RegisterMsg(ScriptClass* scriptThis, bool (ScriptClass::*scriptHandler)(Entity&, DerivedMsgType&)) {
		static_assert(std::is_base_of_v<GameMessages::GameMsg, DerivedMsgType>, "DerivedMsgType must derive from GameMessages::GameMsg base class.");
		const auto boundMsg = std::bind(scriptHandler, scriptThis, std::placeholders::_1, std::placeholders::_2);
		auto* const parent = m_Parent;
		const auto castWrapper = [parent, boundMsg](GameMessages::GameMsg& msg) {
			return boundMsg(*parent, static_cast<DerivedMsgType&>(msg));
			};
		DerivedMsgType msg;
		m_Parent->RegisterMsg(msg.msgId, castWrapper);
	}

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
