#ifndef __BLOCKDEFINITION__H__
#define __BLOCKDEFINITION__H__

#include <string>

class AMFArrayValue;

class BlockDefinition {
public:
	BlockDefinition(const std::string_view defaultValue = "", const float minimumValue = 0.0f, const float maximumValue = 0.0f);
	static BlockDefinition blockDefinitionDefault;

	[[nodiscard]] std::string_view GetDefaultValue() const { return defaultValue; };
	[[nodiscard]] float GetMinimumValue() const noexcept { return minimumValue; };
	[[nodiscard]] float GetMaximumValue() const noexcept { return maximumValue; };
	void SetDefaultValue(const std::string_view value) { defaultValue = std::string{ value }; };
	void SetMinimumValue(const float value) noexcept { minimumValue = value; };
	void SetMaximumValue(const float value) noexcept { maximumValue = value; };

private:
	std::string defaultValue;
	float minimumValue;
	float maximumValue;
};

#endif  //!__BLOCKDEFINITION__H__
