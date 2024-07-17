#include "ModifierType.h"
#include <unordered_map>
#include <unordered_set>

using namespace nejlika;

namespace
{

const std::unordered_map<ModifierType, std::string> colorMap = {
	{ModifierType::Health, "#750000"},
	{ModifierType::Armor, "#525252"},
	{ModifierType::Imagination, "#0077FF"},
	{ModifierType::Offensive, "#71583B"},
	{ModifierType::Defensive, "#71583B"},
	{ModifierType::Physical, "#666666"},
	{ModifierType::Pierce, "#4f4f4f"},
	{ModifierType::Vitality, "#e84646"},
	{ModifierType::Fire, "#ff0000"},
	{ModifierType::Cold, "#94d0f2"},
	{ModifierType::Lightning, "#00a2ff"},
	{ModifierType::Corruption, "#3d00ad"},
	{ModifierType::Psychic, "#4b0161"},
	{ModifierType::Acid, "#00ff00"},
};

const std::unordered_map<ModifierType, std::string> nameMap = {
	{ModifierType::Health, "Health"},
	{ModifierType::Armor, "Armor"},
	{ModifierType::Imagination, "Imagination"},
	{ModifierType::Offensive, "Offensive Ability"},
	{ModifierType::Defensive, "Defensive Ability"},
	{ModifierType::Physical, "Physical Damage"},
	{ModifierType::Pierce, "Pierce Damage"},
	{ModifierType::Vitality, "Vitality Damage"},
	{ModifierType::Fire, "Fire Damage"},
	{ModifierType::Cold, "Cold Damage"},
	{ModifierType::Lightning, "Lightning Damage"},
	{ModifierType::Corruption, "Corruption Damage"},
	{ModifierType::Psychic, "Psychic Damage"},
	{ModifierType::Acid, "Acid Damage"},
	{ModifierType::InternalDisassembly, "Internal Disassembly"},
	{ModifierType::InternalDisassemblyDuration, "Internal Disassembly Duration"},
	{ModifierType::Burn, "Burn"},
	{ModifierType::BurnDuration, "Burn Duration"},
	{ModifierType::Frostburn, "Frostburn"},
	{ModifierType::FrostburnDuration, "Frostburn Duration"},
	{ModifierType::Poison, "Poison"},
	{ModifierType::PoisonDuration, "Poison Duration"},
	{ModifierType::Electrocute, "Electrocute"},
	{ModifierType::ElectrocuteDuration, "Electrocute Duration"},
	{ModifierType::VitalityDecay, "Vitality Decay"},
	{ModifierType::VitalityDecayDuration, "Vitality Decay Duration"},
	{ModifierType::Seperation, "Seperation"},
	{ModifierType::SeperationDuration, "Seperation Duration"},
	{ModifierType::Elemental, "Elemental Damage"},
	{ModifierType::Damage, "Damage"},
	{ModifierType::Speed, "Speed"},
	{ModifierType::AttackSpeed, "Attack Speed"},
	{ModifierType::BlockRecovery, "Block Recovery Speed"},
	{ModifierType::BlockChance, "Chance to Block"},
	{ModifierType::Block, "Damage to Block"},
	{ModifierType::CriticalDamage, "Critical-hit Damage"},
	{ModifierType::HealthDrain, "Damage converted to Health"},
	{ModifierType::ArmorPiercing, "Armor Piercing"},
	{ModifierType::ReducedStunDuration, "Reduced Stun Duration"},
	{ModifierType::SkillCooldownReduction, "Skill Cooldown Reduction"},
	{ModifierType::SkillRecharge, "Skill Recharge Time"},
	{ModifierType::Slow, "Slow"},
	{ModifierType::Physique, "Physique"},
	{ModifierType::Cunning, "Cunning"},
	{ModifierType::Spirit, "Imagination"},
	{ModifierType::AttacksPerSecond, "Attacks per Second"},
	{ModifierType::ImaginationCost, "Imagination Cost"},
	{ModifierType::MainWeaponDamage, "Main Weapon Damage"},
	{ModifierType::Stun, "Target Stun Duration"}
};

const std::unordered_map<ModifierType, ModifierType> resistanceMap = {
	{ModifierType::Physical, ModifierType::Physical},
	{ModifierType::Pierce, ModifierType::Pierce},
	{ModifierType::Vitality, ModifierType::Vitality},
	{ModifierType::Fire, ModifierType::Fire},
	{ModifierType::Cold, ModifierType::Cold},
	{ModifierType::Lightning, ModifierType::Lightning},
	{ModifierType::Corruption, ModifierType::Corruption},
	{ModifierType::Psychic, ModifierType::Psychic},
	{ModifierType::Acid, ModifierType::Acid},
	{ModifierType::InternalDisassembly, ModifierType::Physical},
	{ModifierType::Burn, ModifierType::Fire},
	{ModifierType::Frostburn, ModifierType::Cold},
	{ModifierType::Poison, ModifierType::Acid},
	{ModifierType::VitalityDecay, ModifierType::Vitality},
	{ModifierType::Electrocute, ModifierType::Lightning},
	{ModifierType::Seperation, ModifierType::Seperation}
};

const std::unordered_set<ModifierType> normalDamageTypes = {
	ModifierType::Physical,
	ModifierType::Pierce,
	ModifierType::Vitality,
	ModifierType::Fire,
	ModifierType::Cold,
	ModifierType::Lightning,
	ModifierType::Corruption,
	ModifierType::Psychic,
	ModifierType::Acid
};

const std::unordered_map<ModifierType, ModifierType> durationMap = {
	{ModifierType::InternalDisassembly, ModifierType::InternalDisassemblyDuration},
	{ModifierType::Burn, ModifierType::BurnDuration},
	{ModifierType::Frostburn, ModifierType::FrostburnDuration},
	{ModifierType::Poison, ModifierType::PoisonDuration},
	{ModifierType::VitalityDecay, ModifierType::VitalityDecayDuration},
	{ModifierType::Electrocute, ModifierType::ElectrocuteDuration},
	{ModifierType::Seperation, ModifierType::SeperationDuration}
};

const std::unordered_map<ModifierType, ModifierType> overTimeMap = {
	{ModifierType::Physical, ModifierType::InternalDisassembly},
	{ModifierType::Fire, ModifierType::Burn},
	{ModifierType::Cold, ModifierType::Frostburn},
	{ModifierType::Poison, ModifierType::Poison},
	{ModifierType::Vitality, ModifierType::VitalityDecay},
	{ModifierType::Lightning, ModifierType::Electrocute}
};

const std::unordered_set<ModifierType> isOverTimeMap = {
	ModifierType::InternalDisassembly,
	ModifierType::Burn,
	ModifierType::Frostburn,
	ModifierType::Poison,
	ModifierType::VitalityDecay,
	ModifierType::Electrocute,
	ModifierType::Seperation
};

const std::unordered_set<ModifierType> isDurationType = {
	ModifierType::InternalDisassemblyDuration,
	ModifierType::BurnDuration,
	ModifierType::FrostburnDuration,
	ModifierType::PoisonDuration,
	ModifierType::VitalityDecayDuration,
	ModifierType::ElectrocuteDuration,
	ModifierType::SeperationDuration
};

}

const std::string& nejlika::GetModifierTypeColor(ModifierType type)
{
	const auto color = colorMap.find(type);

	if (color != colorMap.end()) {
		return color->second;
	}

	static const std::string white = "#FFFFFF";

	return white;
}

const std::string& nejlika::GetModifierTypeName(ModifierType type) {
	const auto name = nameMap.find(type);

	if (name != nameMap.end()) {
		return name->second;
	}

	static const std::string invalid = "Invalid";

	return invalid;
}

const ModifierType nejlika::GetResistanceType(ModifierType type) {
	const auto resistance = resistanceMap.find(type);

	if (resistance != resistanceMap.end()) {
		return resistance->second;
	}

	return ModifierType::Invalid;
}

const bool nejlika::IsNormalDamageType(ModifierType type) {
	return normalDamageTypes.find(type) != normalDamageTypes.end();
}

const ModifierType nejlika::GetOverTimeType(ModifierType type) {
	const auto overTime = overTimeMap.find(type);

	if (overTime != overTimeMap.end()) {
		return overTime->second;
	}

	return ModifierType::Invalid;
}

const ModifierType nejlika::GetDurationType(ModifierType type) {
	const auto duration = durationMap.find(type);

	if (duration != durationMap.end()) {
		return duration->second;
	}

	return ModifierType::Invalid;
}

const bool nejlika::IsOverTimeType(ModifierType type) {
	return isOverTimeMap.find(type) != isOverTimeMap.end();
}

const bool nejlika::IsDurationType(ModifierType type) {
	return isDurationType.find(type) != isDurationType.end();
}
