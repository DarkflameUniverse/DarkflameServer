#pragma once

#include <cstdint>

#include "json.hpp"

namespace nejlika
{

class ModifierScale
{
public:
	ModifierScale() = default;

	ModifierScale(int32_t level, float min, float max) : level(level), min(min), max(max) {}

	ModifierScale(const nlohmann::json& json);

	nlohmann::json ToJson() const;

	int32_t GetLevel() const { return level; }

	float GetMin() const { return min; }

	float GetMax() const { return max; }

private:
	int32_t level = 0;
	float min = 0.0f;
	float max = 0.0f;
};

}