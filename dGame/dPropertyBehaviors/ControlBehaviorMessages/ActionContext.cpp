#include "ActionContext.h"

#include <stdexcept>

#include "Amf3.h"

ActionContext::ActionContext() {
	stripId = 0;
	stateId = BehaviorState::HOME_STATE;
}

ActionContext::ActionContext(AMFArrayValue* arguments, std::string customStateKey, std::string customStripKey) {
	stripId = 0;
	stateId = BehaviorState::HOME_STATE;
	stripId = GetStripIdFromArgument(arguments, customStripKey);
	stateId = GetBehaviorStateFromArgument(arguments, customStateKey);
}

BehaviorState ActionContext::GetBehaviorStateFromArgument(AMFArrayValue* arguments, const std::string& key) {
	auto* stateIDValue = arguments->Get<double>(key);
	if (!stateIDValue) throw std::invalid_argument("Unable to find behavior state from argument \"" + key + "\"");

	return static_cast<BehaviorState>(stateIDValue->GetValue());
}

StripId ActionContext::GetStripIdFromArgument(AMFArrayValue* arguments, const std::string& key) {
	auto* stripIdValue = arguments->Get<double>(key);
	if (!stripIdValue) throw std::invalid_argument("Unable to find strip ID from argument \"" + key + "\"");

	return static_cast<StripId>(stripIdValue->GetValue());
}
