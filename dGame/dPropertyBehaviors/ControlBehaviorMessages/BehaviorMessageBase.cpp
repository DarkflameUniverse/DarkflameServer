#include "BehaviorMessageBase.h"

#include "Amf3.h"
#include "BehaviorStates.h"
#include "dCommonVars.h"

BehaviorMessageBase::BehaviorMessageBase(AMFArrayValue* const arguments) {
	behaviorId = GetBehaviorIdFromArgument(arguments);
}

int32_t BehaviorMessageBase::GetBehaviorIdFromArgument(AMFArrayValue* const arguments) {
	const char* const key = "BehaviorID";
	const auto* const behaviorIDValue = arguments->Get<std::string>(key);

	if (behaviorIDValue && behaviorIDValue->GetValueType() == eAmf::String) {
		GeneralUtils::TryParse(behaviorIDValue->GetValue(), behaviorId);
	} else if (arguments->Get(key) && arguments->Get(key)->GetValueType() != eAmf::Undefined) {
		throw std::invalid_argument("Unable to find behavior ID");
	}

	return behaviorId;
}

int32_t BehaviorMessageBase::GetActionIndexFromArgument(AMFArrayValue* const arguments, const std::string& keyName) const {
	const auto* const actionIndexAmf = arguments->Get<double>(keyName);
	if (!actionIndexAmf) throw std::invalid_argument("Unable to find actionIndex");

	return static_cast<int32_t>(actionIndexAmf->GetValue());
}
