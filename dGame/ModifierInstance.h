#pragma once

#include "ModifierType.h"
#include "ModifierCategory.h"
#include "ModifierOperator.h"

#include <cstdint>
#include <string>

#include "json.hpp"

namespace nejlika
{

class ModifierInstance
{
public:
	ModifierInstance(
		ModifierType type, float value, ModifierOperator op, bool isResistance, ModifierCategory category, uint32_t effectID, const std::string& effectType
	) : type(type), value(value), op(op), isResistance(isResistance), category(category), effectID(effectID), effectType(effectType) {}

	/**
	 * @brief Construct a new Modifier Instance object from a json configuration.
	 * 
	 * @param config The json configuration.
	 */
	ModifierInstance(const nlohmann::json& config);

	/**
	 * @brief Convert the modifier instance to a json representation.
	 * 
	 * @return The json representation.
	 */
	nlohmann::json ToJson() const;

	/**
	 * @brief Generate a HTML string representation of a set of modifiers.
	 * 
	 * @param modifiers The modifiers to generate the HTML string for.
	 * @return The HTML string.
	 */
	static std::string GenerateHtmlString(const std::vector<ModifierInstance>& modifiers);

	// Getters and setters

	ModifierType GetType() const { return type; }

	float GetValue() const { return value; }
	
	ModifierOperator GetOperator() const { return op; }

	bool IsResistance() const { return isResistance; }
	
	ModifierCategory GetCategory() const { return category; }

	uint32_t GetEffectID() const { return effectID; }

	std::string GetEffectType() const { return effectType; }

	void SetType(ModifierType type) { this->type = type; }

	void SetValue(float value) { this->value = value; }

	void SetOperator(ModifierOperator op) { this->op = op; }

	void SetIsResistance(bool isResistance) { this->isResistance = isResistance; }

	void SetCategory(ModifierCategory category) { this->category = category; }

	void SetEffectID(uint32_t effectID) { this->effectID = effectID; }

	void SetEffectType(const std::string& effectType) { this->effectType = effectType; }

private:
	ModifierType type;
	float value;
	ModifierOperator op;
	bool isResistance;
	ModifierCategory category;
	uint32_t effectID;
	std::string effectType;
};

}
