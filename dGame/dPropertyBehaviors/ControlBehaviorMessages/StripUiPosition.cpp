#include "StripUiPosition.h"

#include "Amf3.h"

StripUiPosition::StripUiPosition() {
	xPosition = 0.0;
	yPosition = 0.0;
}

StripUiPosition::StripUiPosition(AMFArrayValue* arguments, std::string uiKeyName) {
	xPosition = 0.0;
	yPosition = 0.0;
	auto* uiArray = arguments->FindValue<AMFArrayValue>(uiKeyName);
	if (!uiArray) return;

	auto* xPositionValue = uiArray->FindValue<AMFDoubleValue>("x");
	auto* yPositionValue = uiArray->FindValue<AMFDoubleValue>("y");
	if (!xPositionValue || !yPositionValue) return;

	yPosition = yPositionValue->GetValue();
	xPosition = xPositionValue->GetValue();
}
