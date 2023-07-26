#include "CDSkillBehaviorTable.h"

void CDSkillBehaviorTable::LoadValuesFromDatabase() {
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM SkillBehavior");
	while (!tableData.eof()) {
		CDSkillBehavior entry;
		uint32_t skillID = tableData.getIntField("skillID", -1);
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

		this->entries.insert_or_assign(skillID, entry);
		tableData.nextRow();
	}
}

const std::optional<CDSkillBehavior> CDSkillBehaviorTable::GetSkillByID(unsigned int skillID) {
	auto it = this->entries.find(skillID);
	return it != this->entries.end() ? std::make_optional(it->second) : std::nullopt;
}

