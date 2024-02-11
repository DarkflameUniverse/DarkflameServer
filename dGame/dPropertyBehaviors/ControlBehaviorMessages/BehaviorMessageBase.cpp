#include "BehaviorMessageBase.h"

#include "Amf3.h"
#include "BehaviorStates.h"
#include "dCommonVars.h"

BehaviorMessageBase::BehaviorMessageBase(const AMFArrayValue& arguments) {
	m_BehaviorId = GetBehaviorIdFromArgument(arguments);
}

int32_t BehaviorMessageBase::GetBehaviorIdFromArgument(const AMFArrayValue& arguments) {
	static constexpr std::string_view key = "BehaviorID";
	const auto* const behaviorIDValue = arguments.Get<std::string>(key);

	if (behaviorIDValue && behaviorIDValue->GetValueType() == eAmf::String) {
		m_BehaviorId =
			GeneralUtils::TryParse<int32_t>(behaviorIDValue->GetValue()).value_or(m_BehaviorId);
	} else if (arguments.Get(key) && arguments.Get(key)->GetValueType() != eAmf::Undefined) {
		throw std::invalid_argument("Unable to find behavior ID");
	}

	return m_BehaviorId;
}

int32_t BehaviorMessageBase::GetActionIndexFromArgument(const AMFArrayValue& arguments, const std::string_view keyName) const {
	const auto* const actionIndexAmf = arguments.Get<double>(keyName);
	if (!actionIndexAmf) throw std::invalid_argument("Unable to find actionIndex");

	return static_cast<int32_t>(actionIndexAmf->GetValue());
}
