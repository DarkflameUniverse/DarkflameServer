#include "BehaviorMessageBase.h"

#include "Amf3.h"
#include "BehaviorStates.h"
#include "dCommonVars.h"

BehaviorMessageBase::BehaviorMessageBase(AMFArrayValue* arguments) {
	this->behaviorId = GetBehaviorIdFromArgument(arguments);
}

int32_t BehaviorMessageBase::GetBehaviorIdFromArgument(AMFArrayValue* arguments) {
	const auto* key = "BehaviorID";
	auto* behaviorIDValue = arguments->Get<std::string>(key);

	if (behaviorIDValue && behaviorIDValue->GetValueType() == eAmf::String) {
		this->behaviorId =
			GeneralUtils::TryParse<int32_t>(behaviorIDValue->GetValue()).value_or(this->behaviorId);
	} else if (arguments->Get(key) && arguments->Get(key)->GetValueType() != eAmf::Undefined) {
		throw std::invalid_argument("Unable to find behavior ID");
	}

	return this->behaviorId;
}

int32_t BehaviorMessageBase::GetActionIndexFromArgument(AMFArrayValue* arguments, const std::string& keyName) {
	auto* actionIndexAmf = arguments->Get<double>(keyName);
	if (!actionIndexAmf) {
		throw std::invalid_argument("Unable to find actionIndex");
	}

	return static_cast<int32_t>(actionIndexAmf->GetValue());
}
