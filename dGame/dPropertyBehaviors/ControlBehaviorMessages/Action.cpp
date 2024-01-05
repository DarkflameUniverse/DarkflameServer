#include "Action.h"
#include "Amf3.h"

Action::Action() {	
	type = "";
	valueParameterName = "";
	valueParameterString = "";
	valueParameterDouble = 0.0;
}

Action::Action(AMFArrayValue* arguments) {
	type = "";
	valueParameterName = "";
	valueParameterString = "";
	valueParameterDouble = 0.0;
	for (auto& [paramName, paramValue] : arguments->GetAssociative()) {
		if (paramName == "Type") {
			if (paramValue->GetValueType() != eAmf::String) continue;
			type = static_cast<AMFStringValue*>(paramValue)->GetValue();
		} else {
			valueParameterName = paramName;
			// Message is the only known string parameter
			if (valueParameterName == "Message") {
				if (paramValue->GetValueType() != eAmf::String) continue;
				valueParameterString = static_cast<AMFStringValue*>(paramValue)->GetValue();
			} else {
				if (paramValue->GetValueType() != eAmf::Double) continue;
				valueParameterDouble = static_cast<AMFDoubleValue*>(paramValue)->GetValue();
			}
		}
	}
}

void Action::SendBehaviorBlocksToClient(AMFArrayValue& args) const {
	auto* actionArgs = args.PushArray();
	actionArgs->Insert("Type", type);

	auto valueParameterName = GetValueParameterName();
	if (valueParameterName.empty()) return;

	if (valueParameterName == "Message") {
		actionArgs->Insert(valueParameterName, valueParameterString);
	} else {
		actionArgs->Insert(valueParameterName, valueParameterDouble);
	}
}
