#include "AddActionMessage.h"

AddActionMessage::AddActionMessage(AMFArrayValue* arguments) {
	auto* actionIndexAmf = arguments->FindValue<AMFDoubleValue>("actionIndex");
	if (!actionIndexAmf) return;

	actionIndex = static_cast<uint32_t>(actionIndexAmf->GetDoubleValue());

	stripId = GetStripIdFromArgument(arguments);

	stateId = GetBehaviorStateFromArgument(arguments);

	type = "";
	valueParameterName = "";
	valueParameterString = "";
	valueParameterDouble = 0.0;
	auto* action = arguments->FindValue<AMFArrayValue>("action");
	if (!action) return;

	for (auto& typeValueMap : action->GetAssociativeMap()) {
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

	behaviorId = GetBehaviorIDFromArgument(arguments);
	Game::logger->LogDebug("AddActionMessage", "actionIndex %i stripId %i stateId %i type %s valueParameterName %s valueParameterString %s valueParameterDouble %f behaviorId %i", actionIndex, stripId, stateId, type.c_str(), valueParameterName.c_str(), valueParameterString.c_str(), valueParameterDouble, behaviorId);
}
