#include "UpdateStripUiMessage.h"

UpdateStripUiMessage::UpdateStripUiMessage(AMFArrayValue* arguments) {
	auto* uiArray = arguments->FindValue<AMFArrayValue>("ui");
	if (!uiArray) return;

	auto* xPositionValue = uiArray->FindValue<AMFDoubleValue>("x");
	auto* yPositionValue = uiArray->FindValue<AMFDoubleValue>("y");
	if (!xPositionValue || !yPositionValue) return;

	yPosition = yPositionValue->GetDoubleValue();
	xPosition = xPositionValue->GetDoubleValue();

	stripID = GetStripIDFromArgument(arguments);

	stateID = GetBehaviorStateFromArgument(arguments);

	behaviorID = GetBehaviorIDFromArgument(arguments);
	Game::logger->LogDebug("UpdateStripUIMessage", "x %f y %f stpId %i sttId %i bhId %i", xPosition, yPosition, stripID, stateID, behaviorID);
}
