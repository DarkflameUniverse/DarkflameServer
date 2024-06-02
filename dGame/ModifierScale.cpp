#include "ModifierScale.h"

nejlika::ModifierScale::ModifierScale(const nlohmann::json & json)
{
	level = json["level"].get<int32_t>();

	if (json.contains("min")) {
		min = json["min"].get<float>();
	}
	else {
		min = 0.0f;
	}

	if (json.contains("max")) {
		max = json["max"].get<float>();
	}
	else {
		max = 0.0f;
	}
	
	if (json.contains("value")) {
		min = json["value"].get<float>();
		max = json["value"].get<float>();
	}
}

nlohmann::json nejlika::ModifierScale::ToJson() const
{
	nlohmann::json json;

	json["level"] = level;
	json["min"] = min;
	json["max"] = max;

	return json;
}
