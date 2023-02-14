#include "StripUiPosition.h"

#include "AMFFormat.h"

StripUiPosition::StripUiPosition() {
	xPosition = 0.0;
	yPosition = 0.0;
}

StripUiPosition::StripUiPosition(AMFArrayValue* arguments, std::string uiKeyName) {
	auto* uiArray = arguments->FindValue<AMFArrayValue>(uiKeyName);
	if (!uiArray) return;

	auto* xPositionValue = uiArray->FindValue<AMFDoubleValue>("x");
	auto* yPositionValue = uiArray->FindValue<AMFDoubleValue>("y");
	if (!xPositionValue || !yPositionValue) return;

	yPosition = yPositionValue->GetDoubleValue();
	xPosition = xPositionValue->GetDoubleValue();
}
