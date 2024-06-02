#include "ModifierScale.h"

nejlika::ModifierScale::ModifierScale(const nlohmann::json & json)
{
	level = json["level"].get<int32_t>();
	min = json["min"].get<float>();
	max = json["max"].get<float>();
}

nlohmann::json nejlika::ModifierScale::ToJson() const
{
	nlohmann::json json;

	json["level"] = level;
	json["min"] = min;
	json["max"] = max;

	return json;
}
