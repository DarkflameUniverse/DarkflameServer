#pragma once

#include <cstdint>
#include <vector>

#include "Entity.h"

#include "ModifierInstance.h"
#include "EntityTemplate.h"

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

	void ApplyToEntity();

	void CheckForRescale(AdditionalEntityData* other);

	int32_t GetLevel() const { return level; }

	LWOOBJID GetID() const { return id; }

	LOT GetLOT() const { return lot; }

private:
	void RollStandardModifiers(int32_t level);

	bool initialized = false;

	std::vector<ModifierInstance> standardModifiers;
	std::vector<ModifierInstance> activeModifiers;

	LWOOBJID id;
	LOT lot;

	int32_t level = 1;
};

}
