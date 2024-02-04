#include "RemoveStripMessage.h"

RemoveStripMessage::RemoveStripMessage(AMFArrayValue* arguments) : BehaviorMessageBase(arguments) {
	actionContext = ActionContext(arguments);

	LOG_DEBUG("stripId %i stateId %i m_BehaviorId %i", actionContext.GetStripId(), actionContext.GetStateId(), m_BehaviorId);
}
