#include "CDSkillBehaviorTable.h"

namespace {
	CDSkillBehavior m_empty = CDSkillBehavior();
};

void CDSkillBehaviorTable::LoadValuesFromDatabase() {
	// First, get the size of the table
	uint32_t size = 0;
	auto tableSize = CDClientDatabase::ExecuteQuery("SELECT COUNT(*) FROM SkillBehavior");
	while (!tableSize.eof()) {
		size = tableSize.getIntField(0, 0);

		tableSize.nextRow();
	}

	tableSize.finalize();

	auto& entries = GetEntriesMutable();

	// Now get the data
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM SkillBehavior");
	while (!tableData.eof()) {
		CDSkillBehavior entry;
		entry.skillID = tableData.getIntField("skillID", -1);
		UNUSED(entry.locStatus = tableData.getIntField("locStatus", -1));
		entry.behaviorID = tableData.getIntField("behaviorID", -1);
		entry.imaginationcost = tableData.getIntField("imaginationcost", -1);
		entry.cooldowngroup = tableData.getIntField("cooldowngroup", -1);
		entry.cooldown = tableData.getFloatField("cooldown", -1.0f);
		UNUSED(entry.isNpcEditor = tableData.getIntField("isNpcEditor", -1) == 1 ? true : false);
		UNUSED(entry.skillIcon = tableData.getIntField("skillIcon", -1));
		UNUSED(entry.oomSkillID = tableData.getStringField("oomSkillID", ""));
		UNUSED(entry.oomBehaviorEffectID = tableData.getIntField("oomBehaviorEffectID", -1));
		UNUSED(entry.castTypeDesc = tableData.getIntField("castTypeDesc", -1));
		UNUSED(entry.imBonusUI = tableData.getIntField("imBonusUI", -1));
		UNUSED(entry.lifeBonusUI = tableData.getIntField("lifeBonusUI", -1));
		UNUSED(entry.armorBonusUI = tableData.getIntField("armorBonusUI", -1));
		UNUSED(entry.damageUI = tableData.getIntField("damageUI", -1));
		UNUSED(entry.hideIcon = tableData.getIntField("hideIcon", -1) == 1 ? true : false);
		UNUSED(entry.localize = tableData.getIntField("localize", -1) == 1 ? true : false);
		UNUSED(entry.gate_version = tableData.getStringField("gate_version", ""));
		UNUSED(entry.cancelType = tableData.getIntField("cancelType", -1));

		entries.insert(std::make_pair(entry.skillID, entry));
		//this->entries.push_back(entry);
		tableData.nextRow();
	}

	tableData.finalize();
}

const CDSkillBehavior& CDSkillBehaviorTable::GetSkillByID(uint32_t skillID) {
	auto& entries = GetEntries();
	auto it = entries.find(skillID);
	if (it != entries.end()) {
		return it->second;
	}

	return m_empty;
}

