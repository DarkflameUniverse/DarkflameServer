/*
 * Darkflame Universe
 * Copyright 2018
 */

#include "Entity.h"
#include "ScriptComponent.h"
#include "CDClientManager.h"
#include "CDScriptComponentTable.h"
#include "GameMessages.h"
#include "Amf3.h"

#include <ranges>

ScriptComponent::ScriptComponent(Entity* parent, const int32_t componentID, const std::string& scriptName, bool serialized, bool client) : Component(parent, componentID) {
	m_Serialized = serialized;
	m_Client = client;
	m_ScriptName = scriptName;

	SetScript(scriptName);
	Component::RegisterMsg(&ScriptComponent::OnGetObjectReportInfo);
}

ScriptComponent::ScriptComponent(Entity* parent, const int32_t componentID) : Component(parent, componentID) {
	m_Serialized = true;
	m_Client = false;
	m_FromObject = true;
	m_Script = nullptr;
	Component::RegisterMsg(&ScriptComponent::OnGetObjectReportInfo);
}

// The ScriptComponent row's server and client scripts (a character has neither)
void ScriptComponent::LoadDataFromTemplate() {
	if (!m_FromObject || m_ComponentID <= 0 || m_Parent->GetCharacter()) return;
	const auto row = CDClientManager::GetTable<CDScriptComponentTable>()->GetByID(m_ComponentID);
	m_ScriptName = row.script_name;
	m_ClientScriptName = row.client_script_name;
}

// custom_script_server replaces a missing server script; a client script (the row's or custom_script_client, or
// any object's without a server script) makes it a client script component
void ScriptComponent::LoadConfigData() {
	if (!m_FromObject) return;
	const bool isCharacter = m_Parent->GetCharacter() != nullptr;
	bool client = false;
	if ((m_ComponentID > 0 || isCharacter) && m_ScriptName.empty() && !isCharacter) client = true;

	const auto customScriptServer = m_Parent->GetVarAsString(u"custom_script_server");
	if (!m_Parent->GetVarAsString(u"custom_script_client").empty()) client = true;
	if (!customScriptServer.empty() && m_ScriptName.empty()) m_ScriptName = customScriptServer;

	m_Client = client && m_ScriptName.empty();
	SetScript(m_ScriptName);
}

void ScriptComponent::Serialize(RakNet::BitStream& outBitStream, bool bIsInitialUpdate) {
	// A server-only script (the zone control object's) sends nothing: the client made no script component for it
	if (!m_Serialized) return;
	if (bIsInitialUpdate) {
		const auto& networkSettings = m_Parent->GetNetworkSettings();
		auto hasNetworkSettings = !networkSettings.values.empty();
		outBitStream.Write(hasNetworkSettings);

		if (hasNetworkSettings) {

			// First write the most inner LDF data
			RakNet::BitStream ldfData;
			ldfData.Write<uint8_t>(0);
			ldfData.Write<uint32_t>(networkSettings.values.size());

			for (const auto& networkSetting : networkSettings.values | std::views::values) {
				networkSetting->WriteToPacket(ldfData);
			}

			// Finally write everything to the stream
			outBitStream.Write<uint32_t>(ldfData.GetNumberOfBytesUsed());
			outBitStream.Write(ldfData);
		}
	}
}

CppScripts::Script* const ScriptComponent::GetScript() {
	return m_Script;
}

void ScriptComponent::SetScript(const std::string& scriptName) {
	// Scripts are managed by the CppScripts class and are effecitvely singletons
	// and they may also be used by other script components so DON'T delete them.
	m_Script = CppScripts::GetScript(m_Parent, scriptName);
}

bool ScriptComponent::OnGetObjectReportInfo(GameMessages::GetObjectReportInfo& reportInfo) {

	auto& scriptInfo = reportInfo.info->PushDebug("Script");
	scriptInfo.PushDebug<AMFStringValue>("Script Name") = m_ScriptName.empty() ? "None" : m_ScriptName;
	auto& networkSettings = scriptInfo.PushDebug("Network Settings");
	for (const auto& setting : m_Parent->GetNetworkSettings().values | std::views::values) {
		networkSettings.PushDebug<AMFStringValue>(GeneralUtils::UTF16ToWTF8(setting->GetKey())) = setting->GetValueAsString();
	}

	return true;
}
