#pragma once

#include <cstdint>
#include <vector>

#include "Entity.h"

#include "ModifierInstance.h"
#include "EntityTemplate.h"
#include "UpgradeTriggerType.h"
#include "TriggerParameters.h"

#include <unordered_set>

namespace nejlika
{

class AdditionalEntityData
{
public:
	AdditionalEntityData() = default;

	AdditionalEntityData(LWOOBJID id, LOT lot) : id(id), lot(lot) {}

	float CalculateModifier(ModifierType type, ModifierOperator op, bool resistance) const;

	float CalculateModifier(ModifierType type, std::vector<ModifierInstance>& additionalModifiers, ModifierOperator op, bool resistance) const;

	float CalculateModifier(ModifierType type, int32_t level) const;

	float CalculateModifier(ModifierType type) const;

	float CalculateModifier(ModifierType type, std::vector<ModifierInstance>& additionalModifiers, int32_t level) const;

	float CalculateResistance(ModifierType type) const;

	/**
	 * @brief Calculate the multiplier for a given modifier type. With a base value of 1.0.
	 * 
	 * @param type The modifier type.
	 * @return The multiplier.
	 */
	float CalculateMultiplier(ModifierType type) const;

	/**
	 * @brief Calculate the multiplier for a given modifier type. With a base value of 1.0.
	 * 
	 * @param type The modifier type.
	 * @param additionalModifiers Additional modifiers to apply.
	 * @return The multiplier.
	 */
	float CalculateMultiplier(ModifierType type, std::vector<ModifierInstance>& additionalModifiers) const;

	/**
	 * @brief Calculate damage conversation mapping.
	 * 
	 * @param additionalModifiers Additional modifiers to apply.
	 * 
	 * @return The damage conversion mapping.
	 */
	std::unordered_map<ModifierType, std::unordered_map<ModifierType, float>> CalculateDamageConversion(std::vector<ModifierInstance>& additionalModifiers) const;

	void ApplyToEntity();

	void CheckForRescale(AdditionalEntityData* other);

	int32_t GetLevel() const { return level; }

	LWOOBJID GetID() const { return id; }

	LOT GetLOT() const { return lot; }

	const std::unordered_set<LWOOBJID>& GetUpgradeItems() const { return upgradeItems; }

	void AddUpgradeItem(LWOOBJID id) { upgradeItems.insert(id); }

	void RemoveUpgradeItem(LWOOBJID id) { upgradeItems.erase(id); }

	std::vector<ModifierInstance> TriggerUpgradeItems(UpgradeTriggerType triggerType, const TriggerParameters& params);

	std::vector<ModifierInstance> TriggerUpgradeItems(UpgradeTriggerType triggerType);

	void InitializeSkills();

	void AddSkills(LWOOBJID item);
	void RemoveSkills(LOT lot);

private:
	void RollStandardModifiers(int32_t level);

	bool initialized = false;

	std::vector<ModifierInstance> standardModifiers;
	std::vector<ModifierInstance> activeModifiers;
	std::unordered_set<LWOOBJID> upgradeItems;

	LWOOBJID id;
	LOT lot;

	int32_t level = 1;
};

}
