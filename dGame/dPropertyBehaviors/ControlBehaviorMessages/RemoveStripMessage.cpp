#include "RemoveStripMessage.h"

RemoveStripMessage::RemoveStripMessage(AMFArrayValue* arguments) : BehaviorMessageBase(arguments) {
	actionContext = ActionContext(arguments);

	Game::logger->LogDebug("RemoveStripMessage", "stripId %i stateId %i behaviorId %i", actionContext.GetStripId(), actionContext.GetStateId(), behaviorId);
}
