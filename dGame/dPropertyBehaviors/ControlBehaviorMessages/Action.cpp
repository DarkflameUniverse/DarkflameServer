#include "Action.h"
#include "Amf3.h"

Action::Action(const AMFArrayValue& arguments) {
	for (const auto& [paramName, paramValue] : arguments.GetAssociative()) {
		if (paramName == "Type") {
			if (paramValue->GetValueType() != eAmf::String) continue;
			m_Type = static_cast<AMFStringValue*>(paramValue.get())->GetValue();
		} else {
			m_ValueParameterName = paramName;
			// Message is the only known string parameter
			if (m_ValueParameterName == "Message") {
				if (paramValue->GetValueType() != eAmf::String) continue;
				m_ValueParameterString = static_cast<AMFStringValue*>(paramValue.get())->GetValue();
			} else {
				if (paramValue->GetValueType() != eAmf::Double) continue;
				m_ValueParameterDouble = static_cast<AMFDoubleValue*>(paramValue.get())->GetValue();
			}
		}
	}
}

void Action::SendBehaviorBlocksToClient(AMFArrayValue& args) const {
	auto* const actionArgs = args.PushArray();
	actionArgs->Insert("Type", m_Type);

	if (m_ValueParameterName.empty()) return;

	if (m_ValueParameterName == "Message") {
		actionArgs->Insert(m_ValueParameterName, m_ValueParameterString);
	} else {
		actionArgs->Insert(m_ValueParameterName, m_ValueParameterDouble);
	}
}
