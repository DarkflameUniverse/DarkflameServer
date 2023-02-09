#include "BlockDefinition.h"

BlockDefinition BlockDefinition::blockDefinitionDefault{};

BlockDefinition::BlockDefinition(std::string defaultValue, float minimumValue, float maximumValue) {
	this->defaultValue = defaultValue;
	this->minimumValue = minimumValue;
	this->maximumValue = maximumValue;
}
