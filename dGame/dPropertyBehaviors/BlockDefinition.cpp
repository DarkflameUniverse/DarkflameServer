#include "BlockDefinition.h"

BlockDefinition BlockDefinition::blockDefinitionDefault{};

BlockDefinition::BlockDefinition(const std::string_view defaultValue, const float minimumValue, const float maximumValue)
	: defaultValue{ defaultValue }
	, minimumValue{ minimumValue }
	, maximumValue{ maximumValue } {
}
