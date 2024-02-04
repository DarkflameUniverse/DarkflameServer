#include "RemoveActionsMessage.h"

RemoveActionsMessage::RemoveActionsMessage(AMFArrayValue* arguments) : BehaviorMessageBase(arguments) {
	actionContext = ActionContext(arguments);
	actionIndex = GetActionIndexFromArgument(arguments);

	LOG_DEBUG("m_BehaviorId %i actionIndex %i stripId %i stateId %i", m_BehaviorId, actionIndex, actionContext.GetStripId(), actionContext.GetStateId());
}
