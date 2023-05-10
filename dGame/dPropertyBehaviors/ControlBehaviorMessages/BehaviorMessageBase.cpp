#include "BehaviorMessageBase.h"

#include "Amf3.h"
#include "BehaviorStates.h"
#include "dCommonVars.h"

BehaviorMessageBase::BehaviorMessageBase(AMFArrayValue* arguments) {
	behaviorId = 0;
	behaviorId = GetBehaviorIdFromArgument(arguments);
}

int32_t BehaviorMessageBase::GetBehaviorIdFromArgument(AMFArrayValue* arguments) {
	const auto* key = "BehaviorID";
	auto* behaviorIDValue = arguments->Get<std::string>(key);
	int32_t behaviorID = -1;

	if (behaviorIDValue && behaviorIDValue->GetValueType() == eAmf::String) {
		behaviorID = std::stoul(behaviorIDValue->GetValue());
	} else if (arguments->Get(key)->GetValueType() != eAmf::Undefined) {
		throw std::invalid_argument("Unable to find behavior ID");
	}

	return behaviorID;
}

uint32_t BehaviorMessageBase::GetActionIndexFromArgument(AMFArrayValue* arguments, const std::string& keyName) {
	auto* actionIndexAmf = arguments->Get<double>(keyName);
	if (!actionIndexAmf) {
		throw std::invalid_argument("Unable to find actionIndex");
	}

	return static_cast<uint32_t>(actionIndexAmf->GetValue());
}
