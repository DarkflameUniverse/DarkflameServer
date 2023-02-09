#include "RemoveActionsMessage.h"

void RemoveActionsMessage::Parse(AMFArrayValue* arguments) {
	behaviorID = GetBehaviorIDFromArgument(arguments);

	auto* actionIndexAmf = arguments->FindValue<AMFDoubleValue>("actionIndex");
	if (!actionIndexAmf) return;

	actionIndex = static_cast<uint32_t>(actionIndexAmf->GetDoubleValue());

	stripID = GetStripIDFromArgument(arguments);

	stateID = GetBehaviorStateFromArgument(arguments);
	Game::logger->LogDebug("RemoveActionsMessage", "bhId %i acnNdx %i stpId %i sttId %i", behaviorID, actionIndex, stripID, stateID);
}
