/*
 * Darkflame Universe
 * Copyright 2018
 */

#include "Entity.h"
#include "ScriptComponent.h"
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

void ScriptComponent::Serialize(RakNet::BitStream& outBitStream, bool bIsInitialUpdate) {
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
