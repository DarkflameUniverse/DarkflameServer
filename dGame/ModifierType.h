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

	Offensive,
	Defensive,
	
	Slashing,
	Piercing,
	Bludgeoning,

	Fire,
	Cold,
	Lightning,
	Corruption,

	Elemental,

	Psychic,

	Damage,

	Speed,

	AttackSpeed,

	Invalid
};

const std::string& GetModifierTypeColor(ModifierType type);

}
