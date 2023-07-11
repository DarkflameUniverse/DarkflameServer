/*
 * Darkflame Universe
 * Copyright 2018
 */

#include "Entity.h"
#include "ScriptComponent.h"
#include "CDClientManager.h"
#include "CDScriptComponentTable.h"
#include "CDZoneTableTable.h"
#include "dZoneManager.h"

ScriptComponent::ScriptComponent(Entity* parent, const std::string& scriptName) : Component(parent) {
	SetScript(scriptName);
}

void ScriptComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags) {
	if (bIsInitialUpdate) {
		const auto& networkSettings = m_ParentEntity->GetNetworkSettings();
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
	m_Script = CppScripts::GetScript(m_ParentEntity, scriptName);
}

const std::string ScriptComponent::GetScriptName(Entity* parentEntity, const uint32_t componentId) {
	if (!parentEntity || componentId == 0) return "";
	// LDF key script overrides script component Id
	const auto customScriptServer = parentEntity->GetVarAsString(u"custom_script_server");
	if (!customScriptServer.empty()) return customScriptServer;

	auto* scriptCompTable = CDClientManager::Instance().GetTable<CDScriptComponentTable>();
	CDScriptComponent scriptCompData = scriptCompTable->GetByID(componentId);
	return scriptCompData.script_name;
}

const std::string ScriptComponent::GetZoneScriptName(const uint32_t componentId) {
	auto* zoneTable = CDClientManager::Instance().GetTable<CDZoneTableTable>();
	const auto zoneID = dZoneManager::Instance()->GetZoneID();
	const auto* zoneData = zoneTable->Query(zoneID.GetMapID());

	if (!zoneData) return "";

	int zoneScriptID = zoneData->scriptID;
	if (zoneScriptID == -1) return "";
	auto* scriptCompTable = CDClientManager::Instance().GetTable<CDScriptComponentTable>();
	const auto& zoneScriptData = scriptCompTable->GetByID(zoneScriptID);
	return zoneScriptData.script_name;
}
