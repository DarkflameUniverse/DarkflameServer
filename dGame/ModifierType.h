#pragma once

#include <cstdint>
#include <string>

namespace nejlika
{

enum class ModifierType : uint8_t
{
	Health,
	Armor,
	Imagination,

	Physique,
	Cunning,
	Spirit,

	Offensive,
	Defensive,
	
	// Normal Types
	Physical,
	Fire,
	Cold,
	Lightning,
	Acid,
	Vitality,
	Pierce,
	Corruption, // Aether
	Psychic, // Chaos

	// Duration Types
	InternalDisassembly, // Internal Trauma
	InternalDisassemblyDuration,
	Burn,
	BurnDuration,
	Frostburn,
	FrostburnDuration,
	Poison,
	PoisonDuration,
	VitalityDecay,
	VitalityDecayDuration,
	Electrocute,
	ElectrocuteDuration,
	Seperation, // Bleeding
	SeperationDuration,
	
	// Special
	Elemental, // Even split between Fire, Cold, Lightning

	Damage,

	Speed,

	AttackSpeed,
	SkillModifier,

	Slow,
	ArmorPiercing,
	ReducedStunDuration,
	SkillCooldownReduction,
	SkillRecharge,
	BlockRecovery,
	BlockChance,
	Block,
	HealthRegen,
	ImaginationRegen,
	AttacksPerSecond,
	ImaginationCost,
	MainWeaponDamage,
	Stun,

	CriticalDamage,
	HealthDrain,

	Invalid
};

const std::string& GetModifierTypeColor(ModifierType type);

const std::string& GetModifierTypeName(ModifierType type);

const ModifierType GetResistanceType(ModifierType type);

const bool IsNormalDamageType(ModifierType type);

const bool IsOverTimeType(ModifierType type);

const bool IsDurationType(ModifierType type);

const ModifierType GetOverTimeType(ModifierType type);

const ModifierType GetDurationType(ModifierType type);

}
