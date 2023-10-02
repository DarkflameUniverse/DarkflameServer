#include "RemoveActionsMessage.h"

RemoveActionsMessage::RemoveActionsMessage(AMFArrayValue* arguments) : BehaviorMessageBase(arguments) {
	actionContext = ActionContext(arguments);
	actionIndex = GetActionIndexFromArgument(arguments);

	LOG_DEBUG("behaviorId %i actionIndex %i stripId %i stateId %i", behaviorId, actionIndex, actionContext.GetStripId(), actionContext.GetStateId());
}
