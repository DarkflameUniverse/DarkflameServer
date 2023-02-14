#include "UpdateStripUiMessage.h"

UpdateStripUiMessage::UpdateStripUiMessage(AMFArrayValue* arguments) {
	auto* uiArray = arguments->FindValue<AMFArrayValue>("ui");
	if (!uiArray) return;

	auto* xPositionValue = uiArray->FindValue<AMFDoubleValue>("x");
	auto* yPositionValue = uiArray->FindValue<AMFDoubleValue>("y");
	if (!xPositionValue || !yPositionValue) return;

	yPosition = yPositionValue->GetDoubleValue();
	xPosition = xPositionValue->GetDoubleValue();

	stripId = GetStripIdFromArgument(arguments);

	stateID = GetBehaviorStateFromArgument(arguments);

	behaviorId = GetBehaviorIDFromArgument(arguments);
	Game::logger->LogDebug("UpdateStripUIMessage", "xPosition %f yPosition %f stripId %i stateId %i behaviorId %i", xPosition, yPosition, stripId, stateID, behaviorId);
}
