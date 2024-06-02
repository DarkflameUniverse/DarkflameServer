#pragma once

#include <cstdint>
#include <vector>

#include "Entity.h"
#include "ModifierType.h"
#include "ModifierInstance.h"

#include "json.hpp"

namespace nejlika
{

class EntityTemplate
{
public:
	EntityTemplate() = default;

	EntityTemplate(const nlohmann::json& json);

	nlohmann::json ToJson() const;

	LOT GetLOT() const { return lot; }

	int32_t GetMinLevel() const { return minLevel; }

	float GetScaler(ModifierType type, bool isResistance, int32_t level) const;

	std::vector<ModifierInstance> GenerateModifiers(int32_t level) const;

private:
	struct EntityTemplateScaler
	{
		ModifierType type;
		bool isResistance;
		std::vector<float> polynomial;
	};
	
	float CalculateScaler(const EntityTemplateScaler& scaler, int32_t level) const;

	LOT lot;
	std::vector<EntityTemplateScaler> scalers;
	int32_t minLevel;
};

}
