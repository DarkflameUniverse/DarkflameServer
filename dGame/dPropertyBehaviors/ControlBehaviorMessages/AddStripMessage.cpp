#include "AddStripMessage.h"

AddStripMessage::AddStripMessage(AMFArrayValue* arguments) {
	auto* strip = arguments->FindValue<AMFArrayValue>("strip");
	if (!strip) return;

	auto* actions = strip->FindValue<AMFArrayValue>("actions");
	if (!actions) return;

	auto* uiArray = arguments->FindValue<AMFArrayValue>("ui");
	if (!uiArray) return;

	auto* xPositionValue = uiArray->FindValue<AMFDoubleValue>("x");
	if (!xPositionValue) return;

	xPosition = xPositionValue->GetDoubleValue();

	auto* yPositionValue = uiArray->FindValue<AMFDoubleValue>("y");
	if (!yPositionValue) return;

	yPosition = yPositionValue->GetDoubleValue();

	stripId = GetStripIdFromArgument(arguments);

	stateId = GetBehaviorStateFromArgument(arguments);

	behaviorId = GetBehaviorIDFromArgument(arguments);

	type = "";
	valueParameterName = "";
	valueParameterString = "";
	valueParameterDouble = 0.0;
	for (uint32_t position = 0; position < actions->GetDenseValueSize(); position++) {
		auto* actionAsArray = actions->GetValueAt<AMFArrayValue>(position);
		if (!actionAsArray) continue;

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
		Game::logger->LogDebug("AddStripMessage", "xPosition %f yPosition %f stripId %i stateId %i behaviorId %i t %s valueParameterName %s valueParameterString %s valueParameterDouble %f", xPosition, yPosition, stripId, stateId, behaviorId, type.c_str(), valueParameterName.c_str(), valueParameterString.c_str(), valueParameterDouble);
	}
}
