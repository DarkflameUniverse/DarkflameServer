#include "BlockDefinition.h"

BlockDefinition BlockDefinition::blockDefinitionDefault{};

BlockDefinition::BlockDefinition(const std::string& defaultValue, const float minimumValue, const float maximumValue)
	: m_DefaultValue{ defaultValue }
	, m_MinimumValue{ minimumValue }
	, m_MaximumValue{ maximumValue } {
}
