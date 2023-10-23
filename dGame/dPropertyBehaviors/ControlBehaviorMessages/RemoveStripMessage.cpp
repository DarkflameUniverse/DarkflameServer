#include "RemoveStripMessage.h"

RemoveStripMessage::RemoveStripMessage(AMFArrayValue* arguments) : BehaviorMessageBase(arguments) {
	actionContext = ActionContext(arguments);

	LOG_DEBUG("stripId %i stateId %i behaviorId %i", actionContext.GetStripId(), actionContext.GetStateId(), behaviorId);
}
