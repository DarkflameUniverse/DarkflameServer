#include "RemoveActionsMessage.h"

RemoveActionsMessage::RemoveActionsMessage(AMFArrayValue* arguments) {
	behaviorId = GetBehaviorIDFromArgument(arguments);

	auto* actionIndexAmf = arguments->FindValue<AMFDoubleValue>("actionIndex");
	if (!actionIndexAmf) return;

	actionIndex = static_cast<uint32_t>(actionIndexAmf->GetDoubleValue());

	stripId = GetStripIdFromArgument(arguments);

	stateID = GetBehaviorStateFromArgument(arguments);
	Game::logger->LogDebug("RemoveActionsMessage", "behaviorId %i actionIndex %i stripId %i stateId %i", behaviorId, actionIndex, stripId, stateID);
}
