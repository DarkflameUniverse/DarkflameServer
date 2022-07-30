/*
 * Darkflame Universe
 * Copyright 2018
 */

#include "Entity.h"
#include "ScriptComponent.h"

ScriptComponent::ScriptComponent(Entity* parent, std::string scriptName, bool serialized, bool client) : Component(parent) {
	m_Serialized = serialized;
	m_Client = client;

	SetScript(scriptName);
}

ScriptComponent::~ScriptComponent() {

}

void ScriptComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags) {
	if (bIsInitialUpdate) {
		const auto& networkSettings = m_Parent->GetNetworkSettings();
		auto hasNetworkSettings = !networkSettings.empty();
		outBitStream->Write(hasNetworkSettings);

		if (hasNetworkSettings) {

			// First write the most inner LDF data
			RakNet::BitStream ldfData;
			ldfData.Write<uint8_t>(0);
			ldfData.Write<uint32_t>(networkSettings.size());

			for (auto* networkSetting : networkSettings) {
				networkSetting->WriteToPacket(&ldfData);
			}

			// Finally write everything to the stream
			outBitStream->Write<uint32_t>(ldfData.GetNumberOfBytesUsed());
			outBitStream->Write(ldfData);
		}
	}
}

CppScripts::Script* ScriptComponent::GetScript() {
	return m_Script;
}

void ScriptComponent::SetScript(const std::string& scriptName) {
	//we don't need to delete the script because others may be using it :)
	/*if (m_Client) {
		m_Script = new InvalidScript();
		return;
	}*/

	m_Script = CppScripts::GetScript(m_Parent, scriptName);
}
