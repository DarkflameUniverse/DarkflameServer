#include "Action.h"

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
	for (auto& typeValueMap : arguments->GetAssociativeMap()) {
		if (typeValueMap.first == "Type") {
			if (typeValueMap.second->GetValueType() != AMFValueType::AMFString) continue;
			type = static_cast<AMFStringValue*>(typeValueMap.second)->GetStringValue();
		} else {
			valueParameterName = typeValueMap.first;
			// Message is the only known string parameter
			if (valueParameterName == "Message") {
				if (typeValueMap.second->GetValueType() != AMFValueType::AMFString) continue;
				valueParameterString = static_cast<AMFStringValue*>(typeValueMap.second)->GetStringValue();
			} else {
				if (typeValueMap.second->GetValueType() != AMFValueType::AMFDouble) continue;
				valueParameterDouble = static_cast<AMFDoubleValue*>(typeValueMap.second)->GetDoubleValue();
			}
		}
	}
}
