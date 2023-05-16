#include "RemoveStripMessage.h"

RemoveStripMessage::RemoveStripMessage(AMFArrayValue* arguments) : BehaviorMessageBase(arguments) {
	actionContext = ActionContext(arguments);

	LogDebug("stripId %i stateId %i behaviorId %i", actionContext.GetStripId(), actionContext.GetStateId(), behaviorId);
}
