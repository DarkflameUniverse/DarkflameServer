/*
 * Darkflame Universe
 * Copyright 2018
 */

#include "Entity.h"
#include "ScriptComponent.h"
#include "GameMessages.h"
#include "Amf3.h"

ScriptComponent::ScriptComponent(Entity* parent, const int32_t componentID, const std::string& scriptName, bool serialized, bool client) : Component(parent, componentID) {
	using namespace GameMessages;
	m_Serialized = serialized;
	m_Client = client;
	m_ScriptName = scriptName;

	SetScript(scriptName);
	RegisterMsg<GetObjectReportInfo>(this, &ScriptComponent::OnGetObjectReportInfo);
}

void ScriptComponent::Serialize(RakNet::BitStream& outBitStream, bool bIsInitialUpdate) {
	if (bIsInitialUpdate) {
		const auto& networkSettings = m_Parent->GetNetworkSettings();
		auto hasNetworkSettings = !networkSettings.empty();
		outBitStream.Write(hasNetworkSettings);

		if (hasNetworkSettings) {

			// First write the most inner LDF data
			RakNet::BitStream ldfData;
			ldfData.Write<uint8_t>(0);
			ldfData.Write<uint32_t>(networkSettings.size());

			for (auto* networkSetting : networkSettings) {
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

bool ScriptComponent::OnGetObjectReportInfo(GameMessages::GameMsg& msg) {
	auto& infoMsg = static_cast<GameMessages::GetObjectReportInfo&>(msg);

	auto& scriptInfo = infoMsg.info->PushDebug("Script");
	scriptInfo.PushDebug<AMFStringValue>("Script Name") = m_ScriptName.empty() ? "None" : m_ScriptName;
	auto& networkSettings = scriptInfo.PushDebug("Network Settings");
	for (const auto* const setting : m_Parent->GetNetworkSettings()) {
		networkSettings.PushDebug<AMFStringValue>(GeneralUtils::UTF16ToWTF8(setting->GetKey())) = setting->GetValueAsString();
	}

	return true;
}
