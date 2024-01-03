#include "StripUiPosition.h"

#include "Amf3.h"

StripUiPosition::StripUiPosition() {
	xPosition = 0.0;
	yPosition = 0.0;
}

StripUiPosition::StripUiPosition(AMFArrayValue* arguments, std::string uiKeyName) {
	xPosition = 0.0;
	yPosition = 0.0;
	auto* uiArray = arguments->GetArray(uiKeyName);
	if (!uiArray) return;

	auto* xPositionValue = uiArray->Get<double>("x");
	auto* yPositionValue = uiArray->Get<double>("y");
	if (!xPositionValue || !yPositionValue) return;

	yPosition = yPositionValue->GetValue();
	xPosition = xPositionValue->GetValue();
}

void StripUiPosition::SendBehaviorBlocksToClient(AMFArrayValue& args) const {
	auto* uiArgs = args.InsertArray("ui");
	uiArgs->Insert("x", xPosition);
	uiArgs->Insert("y", yPosition);
}
