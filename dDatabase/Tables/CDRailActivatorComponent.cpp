#include "CDRailActivatorComponent.h"
#include "GeneralUtils.h"

CDRailActivatorComponentTable::CDRailActivatorComponentTable() {
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM RailActivatorComponent;");
	while (!tableData.eof()) {
		CDRailActivatorComponent entry;

		entry.id = tableData.getIntField("id", 0);

		entry.startAnimation = GeneralUtils::ASCIIToUTF16(tableData.getStringField("startAnim", ""));
		entry.loopAnimation = GeneralUtils::ASCIIToUTF16(tableData.getStringField("loopAnim", ""));
		entry.stopAnimation = GeneralUtils::ASCIIToUTF16(tableData.getStringField("stopAnim", ""));
		entry.startSound = GeneralUtils::ASCIIToUTF16(tableData.getStringField("startSound", ""));
		entry.loopSound = GeneralUtils::ASCIIToUTF16(tableData.getStringField("loopSound", ""));
		entry.stopSound = GeneralUtils::ASCIIToUTF16(tableData.getStringField("stopSound", ""));

		std::string loopEffectString(tableData.getStringField("effectIDs", ""));
		entry.loopEffectID = EffectPairFromString(loopEffectString);

		entry.preconditions = tableData.getStringField("preconditions", "-1");

		entry.playerCollision = tableData.getIntField("playerCollision", 0);

		entry.cameraLocked = tableData.getIntField("cameraLocked", 0);

		std::string startEffectString(tableData.getStringField("StartEffectID", ""));
		entry.startEffectID = EffectPairFromString(startEffectString);

		std::string stopEffectString(tableData.getStringField("StopEffectID", ""));
		entry.stopEffectID = EffectPairFromString(stopEffectString);

		entry.damageImmune = tableData.getIntField("DamageImmune", 0);

		entry.noAggro = tableData.getIntField("NoAggro", 0);

		entry.showNameBillboard = tableData.getIntField("ShowNameBillboard", 0);

		m_Entries.push_back(entry);
		tableData.nextRow();
	}

	tableData.finalize();
}

CDRailActivatorComponent CDRailActivatorComponentTable::GetEntryByID(int32_t id) const {
	for (const auto& entry : m_Entries) {
		if (entry.id == id)
			return entry;
	}

	return {};
}

std::vector<CDRailActivatorComponent> CDRailActivatorComponentTable::GetEntries() const {
	return m_Entries;
}

std::pair<uint32_t, std::u16string> CDRailActivatorComponentTable::EffectPairFromString(std::string& str) {
	const auto split = GeneralUtils::SplitString(str, ':');
	if (split.size() == 2) {
		return { std::stoi(split.at(0)), GeneralUtils::ASCIIToUTF16(split.at(1)) };
	}

	return {};
}

