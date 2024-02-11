#ifndef __BLOCKDEFINITION__H__
#define __BLOCKDEFINITION__H__

#include <string>

class AMFArrayValue;

class BlockDefinition {
public:
	BlockDefinition(const std::string_view defaultValue = "", const float minimumValue = 0.0f, const float maximumValue = 0.0f);
	static BlockDefinition blockDefinitionDefault;

	[[nodiscard]] std::string_view GetDefaultValue() const { return m_DefaultValue; };
	[[nodiscard]] float GetMinimumValue() const noexcept { return m_MinimumValue; };
	[[nodiscard]] float GetMaximumValue() const noexcept { return m_MaximumValue; };
	void SetDefaultValue(const std::string_view value) { m_DefaultValue = std::string{ value }; };
	void SetMinimumValue(const float value) noexcept { m_MinimumValue = value; };
	void SetMaximumValue(const float value) noexcept { m_MaximumValue = value; };

private:
	std::string m_DefaultValue;
	float m_MinimumValue;
	float m_MaximumValue;
};

#endif  //!__BLOCKDEFINITION__H__
