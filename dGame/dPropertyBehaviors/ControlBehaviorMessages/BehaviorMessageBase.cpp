#include "BehaviorMessageBase.h"

#include "AMFFormat.h"
#include "BehaviorStates.h"
#include "dCommonVars.h"

BehaviorMessageBase::BehaviorMessageBase(AMFArrayValue* arguments) {
	behaviorId = GetBehaviorIdFromArgument(arguments);
}

uint32_t BehaviorMessageBase::GetBehaviorIdFromArgument(AMFArrayValue* arguments) {
	const auto* key = "BehaviorID";
	auto* behaviorIDValue = arguments->FindValue<AMFStringValue>(key);
	uint32_t behaviorID = -1;

	if (behaviorIDValue) {
		behaviorID = std::stoul(behaviorIDValue->GetStringValue());
	} else if (!arguments->FindValue<AMFUndefinedValue>(key)) {
		throw std::invalid_argument("Unable to find behavior ID");
	}

	return behaviorID;
}

uint32_t BehaviorMessageBase::GetActionIndexFromArgument(AMFArrayValue* arguments, const std::string& keyName) {
	auto* actionIndexAmf = arguments->FindValue<AMFDoubleValue>(keyName);
	if (!actionIndexAmf) {
		throw std::invalid_argument("Unable to find actionIndex");
	}

	return static_cast<uint32_t>(actionIndexAmf->GetDoubleValue());
}
