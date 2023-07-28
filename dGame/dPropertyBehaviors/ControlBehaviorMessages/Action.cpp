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
	for (auto& typeValueMap : arguments->GetAssociative()) {
		if (typeValueMap.first == "Type") {
			if (typeValueMap.second->GetValueType() != eAmf::String) continue;
			type = static_cast<AMFStringValue*>(typeValueMap.second)->GetValue();
		} else {
			valueParameterName = typeValueMap.first;
			// Message is the only known string parameter
			if (valueParameterName == "Message") {
				if (typeValueMap.second->GetValueType() != eAmf::String) continue;
				valueParameterString = static_cast<AMFStringValue*>(typeValueMap.second)->GetValue();
			} else {
				if (typeValueMap.second->GetValueType() != eAmf::Double) continue;
				valueParameterDouble = static_cast<AMFDoubleValue*>(typeValueMap.second)->GetValue();
			}
		}
	}
}
