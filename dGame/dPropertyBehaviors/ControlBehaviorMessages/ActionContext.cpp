#include "ActionContext.h"

#include <stdexcept>

#include "Amf3.h"

ActionContext::ActionContext(const AMFArrayValue& arguments, const std::string_view customStateKey, const std::string_view customStripKey)
	: m_StripId{ GetStripIdFromArgument(arguments, customStripKey) }
	, m_StateId{ GetBehaviorStateFromArgument(arguments, customStateKey) } {
}

BehaviorState ActionContext::GetBehaviorStateFromArgument(const AMFArrayValue& arguments, const std::string_view key) const {
	const auto* const stateIDValue = arguments.Get<double>(key);
	if (!stateIDValue) throw std::invalid_argument("Unable to find behavior state from argument \"" + std::string{ key } + "\"");

	return static_cast<BehaviorState>(stateIDValue->GetValue());
}

StripId ActionContext::GetStripIdFromArgument(const AMFArrayValue& arguments, const std::string_view key) const {
	const auto* const stripIdValue = arguments.Get<double>(key);
	if (!stripIdValue) throw std::invalid_argument("Unable to find strip ID from argument \"" + std::string{ key } + "\"");

	return static_cast<StripId>(stripIdValue->GetValue());
}
