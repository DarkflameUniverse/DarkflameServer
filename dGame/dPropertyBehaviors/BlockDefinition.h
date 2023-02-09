#ifndef __BLOCKDEFINITION__H__
#define __BLOCKDEFINITION__H__

#include <string>

class AMFArrayValue;

class BlockDefinition {
public:
	BlockDefinition(std::string defaultValue = "", float minimumValue = 0.0f, float maximumValue = 0.0f);
	std::string defaultValue;
	float minimumValue;
	float maximumValue;
	static BlockDefinition blockDefinitionDefault;
};

#endif  //!__BLOCKDEFINITION__H__
