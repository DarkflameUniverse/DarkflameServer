#include "UpdateActionMessage.h"

UpdateActionMessage::UpdateActionMessage(AMFArrayValue* arguments) {
	type = "";
	valueParameterName = "";
	valueParameterString = "";
	valueParameterDouble = 0.0;
	auto* actionAsArray = arguments->FindValue<AMFArrayValue>("action");
	if (!actionAsArray) return;
	for (auto& typeValueMap : actionAsArray->GetAssociativeMap()) {
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

	behaviorID = GetBehaviorIDFromArgument(arguments);

	auto* actionIndexValue = arguments->FindValue<AMFDoubleValue>("actionIndex");
	if (!actionIndexValue) return;

	actionIndex = static_cast<uint32_t>(actionIndexValue->GetDoubleValue());

	stripID = GetStripIDFromArgument(arguments);

	stateID = GetBehaviorStateFromArgument(arguments);
	Game::logger->LogDebug("UpdateActionMessage", "t %s vpn %s vps %s vpd %f bhId %i acnNdx %i stpId %i sttId %i", type.c_str(), valueParameterName.c_str(), valueParameterString.c_str(), valueParameterDouble, behaviorID, actionIndex, stripID, stateID);
}
