#ifndef __BLOCKDEFINITION__H__
#define __BLOCKDEFINITION__H__

#include <string>

class AMFArrayValue;

class BlockDefinition {
public:
	BlockDefinition(std::string defaultValue = "", float minimumValue = 0.0f, float maximumValue = 0.0f);
	static BlockDefinition blockDefinitionDefault;

	std::string& GetDefaultValue() { return defaultValue; };
	float GetMinimumValue() { return minimumValue; };
	float GetMaximumValue() { return maximumValue; };
	void SetDefaultValue(std::string value) { defaultValue = value; };
	void SetMinimumValue(float value) { minimumValue = value; };
	void SetMaximumValue(float value) { maximumValue = value; };
private:
	std::string defaultValue;
	float minimumValue;
	float maximumValue;
};

#endif  //!__BLOCKDEFINITION__H__
