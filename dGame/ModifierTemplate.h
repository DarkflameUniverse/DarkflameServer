#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <optional>

#include "ModifierInstance.h"
#include "ModifierScale.h"

namespace nejlika
{

enum ModifierTemplateSelector : uint8_t
{
	One,
	All,
	Two
};

class ModifierTemplate
{
public:
	ModifierTemplate(
		const std::vector<ModifierType>& types, ModifierTemplateSelector selector, ModifierCategory category, bool isResistance, uint32_t effectID, const std::string& effectType
	) : types(types), selector(selector), category(category), isResistance(isResistance), effectID(effectID), effectType(effectType) {}

	/**
	 * @brief Construct a new Modifier Template object from a json configuration.
	 * 
	 * @param config The json configuration.
	 */
	ModifierTemplate(const nlohmann::json& config);

	/**
	 * @brief Convert the modifier template to a json representation.
	 * 
	 * @return The json representation.
	 */
	nlohmann::json ToJson() const;

	std::vector<ModifierInstance> GenerateModifiers(int32_t level) const;

	// Getters and setters

	const std::vector<ModifierType>& GetTypes() const { return types; }

	ModifierType GetConvertTo() const { return convertTo; }

	ModifierTemplateSelector GetSelector() const { return selector; }

	const std::vector<ModifierScale>& GetScales() const { return scales; }

	ModifierCategory GetCategory() const { return category; }

	bool IsResistance() const { return isResistance; }

	uint32_t GetEffectID() const { return effectID; }

	std::string GetEffectType() const { return effectType; }

	void SetTypes(const std::vector<ModifierType>& types) { this->types = types; }

	void SetConvertTo(ModifierType convertTo) { this->convertTo = convertTo; }

	void SetSelector(ModifierTemplateSelector selector) { this->selector = selector; }

	void SetScales(const std::vector<ModifierScale>& scales) { this->scales = scales; }

	void SetCategory(ModifierCategory category) { this->category = category; }

	void SetIsResistance(bool isResistance) { this->isResistance = isResistance; }

	void SetEffectID(uint32_t effectID) { this->effectID = effectID; }

	void SetEffectType(const std::string& effectType) { this->effectType = effectType; }
	
	/**
	 * @brief Generate a HTML string representation of a set of modifier templates.
	 * 
	 * @param modifiers The modifier templates to generate the HTML string for.
	 * @param level The level of the modifier templates.
	 * @return The HTML string.
	 */
	static std::string GenerateHtmlString(const std::vector<ModifierTemplate>& modifiers, int32_t level);


private:
	std::optional<ModifierInstance> GenerateModifier(ModifierType type, int32_t level) const;

	std::vector<ModifierType> types;
	ModifierType convertTo;
	ModifierTemplateSelector selector;
	std::vector<ModifierScale> scales;
	std::vector<float> polynomial;
	ModifierCategory category;
	ModifierOperator operatorType;
	bool isResistance;
	uint32_t effectID;
	std::string effectType;

};

}
