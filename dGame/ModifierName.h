#pragma once

#include <cstdint>
#include <string>

#include "ModifierNameType.h"
#include "ModifierRarity.h"
#include "ModifierNameTemplate.h"

#include "json.hpp"

namespace nejlika
{

class ModifierName
{
public:
	ModifierName(const std::string& name, ModifierNameType type, ModifierRarity rarity) :
		name(name), type(type), rarity(rarity) {}

	ModifierName(const nlohmann::json& json);

	ModifierName(const ModifierNameTemplate& templateData);

	nlohmann::json ToJson() const;

	std::string GenerateHtmlString() const;

	/**
	 * @brief Generate a HTML string representation of a set of names.
	 * 
	 * @param modifiers The names to generate the HTML string for.
	 * @return The HTML string.
	 */
	static std::string GenerateHtmlString(const std::vector<ModifierName>& names);

	// Getters and setters

	const std::string& GetName() const { return name; }

	ModifierNameType GetType() const { return type; }

	ModifierRarity GetRarity() const { return rarity; }

	void SetName(const std::string& name) { this->name = name; }

	void SetType(ModifierNameType type) { this->type = type; }

	void SetRarity(ModifierRarity rarity) { this->rarity = rarity; }

private:
	std::string name;
	ModifierNameType type;
	ModifierRarity rarity;
};

}
